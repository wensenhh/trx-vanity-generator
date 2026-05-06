#include "utils/constants.h"
#include "utils/crypto.h"
#include "utils/pattern.h"
#include "utils/rng.h"
#include "host/cpu_generator.h"
#include "host/gpu_generator.h"
#include <iostream>
#include <iomanip>
#include <chrono>
#include <thread>
#include <csignal>
#include <fstream>
#include <algorithm>
#include <sstream>
#include <vector>

using namespace trx;

std::vector<size_t> parse_size_list(const std::string& csv) {
    std::vector<size_t> values;
    std::stringstream ss(csv);
    std::string item;
    while (std::getline(ss, item, ',')) {
        if (item.empty()) continue;
        values.push_back(std::stoull(item));
    }
    return values;
}

static std::atomic<bool> g_running{true};

void signal_handler(int) {
    g_running = false;
    std::cout << "\n\nShutting down...\n";
}

void print_banner() {
    std::cout << R"(
╔══════════════════════════════════════════════════════════════╗
║           TRON (TRX) Vanity Address Generator                  ║
║                    GPU Accelerated                             ║
╠══════════════════════════════════════════════════════════════╣
║  Phase 1: CPU Version (OpenCL GPU coming in Phase 2-4)      ║
╚══════════════════════════════════════════════════════════════╝
)" << '\n';
}

void print_usage(const char* prog) {
    std::cout << "Usage:\n"
              << "  " << prog << " <pattern_type> <pattern> [options]\n\n"
              << "Pattern Types:\n"
              << "  consecutive <digit> <length>  - e.g., consecutive 8 7  (8888888)\n"
              << "  sequential <start> <length>   - e.g., sequential 1 7   (1234567)\n"
              << "  suffix <string>               - e.g., suffix 8888\n"
              << "  prefix <string>               - e.g., prefix ABC\n"
              << "  contains <string>             - e.g., contains 520\n\n"
              << "Options:\n"
              << "  --gpu                 Use GPU acceleration (OpenCL)\n"
              << "  --batch-size <n>      GPU addresses per batch (default: 65536)\n"
              << "  --auto-tune           Benchmark candidate GPU batch sizes and use the fastest\n"
              << "  --auto-tune-sizes <csv> Candidate batch sizes (default: 32768,65536,131072,262144,524288)\n"
              << "  --auto-tune-batches <n> Batches per auto-tune candidate (default: 3)\n"
              << "  --batches <n>         GPU batch count, then stop (default: 0=infinite)\n"
              << "  --gpu-verify          Recompute matched GPU addresses on CPU for debugging\n"
              << "  --profile             Print GPU per-batch timing breakdown\n"
              << "  --benchmark-json      Print final benchmark metrics as JSON\n"
              << "  --max-attempts <n>    Stop after approximately n attempts (CPU smoke/CI)\n"
              << "  -t, --threads <n>     Number of CPU threads (default: auto)\n"
              << "  -o, --output <file>   Output file for matches\n"
              << "  -v, --verbose         Show progress every second\n"
              << "  -h, --help            Show this help\n\n"
              << "Examples:\n"
              << "  " << prog << " consecutive 8 7\n"
              << "  " << prog << " suffix 5201314 -t 8 -o results.txt\n"
              << "  " << prog << " sequential 1 7 -v\n"
              << "  " << prog << " consecutive 8 7 --gpu\n";
}

int main(int argc, char* argv[]) {
    print_banner();

    if (argc < 3) {
        print_usage(argv[0]);
        return 1;
    }

    // Parse arguments
    std::string pattern_type = argv[1];
    std::string pattern_arg = argv[2];
    std::string pattern_arg2;
    int num_threads = static_cast<int>(std::thread::hardware_concurrency());
    std::string output_file;
    bool verbose = false;

    // Add GPU mode CLI flag
    bool use_gpu = false;
    bool gpu_verify = false;
    bool gpu_profile = false;
    bool benchmark_json = false;
    bool gpu_auto_tune = false;
    size_t gpu_auto_tune_batches = 3;
    std::vector<size_t> gpu_auto_tune_candidates{32768, 65536, 131072, 262144, 524288};
    size_t gpu_batch_size = DEFAULT_BATCH_SIZE;
    size_t gpu_num_batches = 0;
    uint64_t max_attempts = 0;

    for (int i = 3; i < argc; ++i) {
        std::string arg = argv[i];
        if ((arg == "-t" || arg == "--threads") && i + 1 < argc) {
            num_threads = std::stoi(argv[++i]);
        } else if ((arg == "-o" || arg == "--output") && i + 1 < argc) {
            output_file = argv[++i];
        } else if (arg == "-v" || arg == "--verbose") {
            verbose = true;
        } else if (arg == "-h" || arg == "--help") {
            print_usage(argv[0]);
            return 0;
        } else if (arg == "--gpu") {
            use_gpu = true;
        } else if (arg == "--gpu-verify") {
            gpu_verify = true;
        } else if (arg == "--profile") {
            gpu_profile = true;
        } else if (arg == "--benchmark-json") {
            benchmark_json = true;
        } else if (arg == "--auto-tune") {
            use_gpu = true;
            gpu_auto_tune = true;
        } else if (arg == "--auto-tune-batches" && i + 1 < argc) {
            gpu_auto_tune_batches = std::stoull(argv[++i]);
        } else if (arg == "--auto-tune-sizes" && i + 1 < argc) {
            gpu_auto_tune_candidates = parse_size_list(argv[++i]);
        } else if (arg == "--batch-size" && i + 1 < argc) {
            gpu_batch_size = std::stoull(argv[++i]);
        } else if (arg == "--batches" && i + 1 < argc) {
            gpu_num_batches = std::stoull(argv[++i]);
        } else if (arg == "--max-attempts" && i + 1 < argc) {
            max_attempts = std::stoull(argv[++i]);
        } else if (i == 3 && (pattern_type == "consecutive" || pattern_type == "sequential")) {
            pattern_arg2 = arg;
        }
    }

    // Create pattern
    std::unique_ptr<Pattern> pattern;
    try {
        if (pattern_type == "consecutive") {
            char digit = pattern_arg[0];
            size_t length = pattern_arg2.empty() ? 7 : std::stoul(pattern_arg2);
            pattern = std::make_unique<SuffixConsecutivePattern>(digit, length);
        } else if (pattern_type == "sequential") {
            char start = pattern_arg[0];
            size_t length = pattern_arg2.empty() ? 7 : std::stoul(pattern_arg2);
            pattern = std::make_unique<SuffixSequentialPattern>(start, length, true);
        } else if (pattern_type == "suffix") {
            pattern = std::make_unique<SuffixCustomPattern>(pattern_arg);
        } else if (pattern_type == "prefix") {
            pattern = std::make_unique<PrefixCustomPattern>(pattern_arg);
        } else if (pattern_type == "contains") {
            pattern = std::make_unique<ContainsPattern>(pattern_arg);
        } else {
            std::cerr << "Unknown pattern type: " << pattern_type << "\n";
            return 1;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error creating pattern: " << e.what() << "\n";
        return 1;
    }

    std::cout << "Pattern: " << pattern->description() << "\n";
    std::cout << "Mode:    " << (use_gpu ? "GPU (OpenCL)" : "CPU") << "\n";
    if (!use_gpu) {
        std::cout << "Threads: " << num_threads << "\n";
    }
    std::cout << "Output:  " << (output_file.empty() ? "stdout" : output_file) << "\n\n";

    GPUGenerationConfig gpu_config;
    if (pattern_type == "consecutive") {
        gpu_config.gpu_pattern_type = 0; // suffix
        std::string target(std::stoul(pattern_arg2.empty() ? "7" : pattern_arg2), pattern_arg[0]);
        gpu_config.gpu_pattern_len = static_cast<uint32_t>(std::min<size_t>(target.size(), gpu_config.gpu_pattern_chars.size()));
        std::copy_n(target.begin(), gpu_config.gpu_pattern_len, gpu_config.gpu_pattern_chars.begin());
    } else if (pattern_type == "sequential") {
        gpu_config.gpu_pattern_type = 0; // suffix
        size_t length = std::stoul(pattern_arg2.empty() ? "7" : pattern_arg2);
        std::string target;
        target.reserve(length);
        char current = pattern_arg[0];
        for (size_t j = 0; j < length; ++j) {
            target.push_back(current++);
        }
        gpu_config.gpu_pattern_len = static_cast<uint32_t>(std::min<size_t>(target.size(), gpu_config.gpu_pattern_chars.size()));
        std::copy_n(target.begin(), gpu_config.gpu_pattern_len, gpu_config.gpu_pattern_chars.begin());
    } else if (pattern_type == "suffix") {
        gpu_config.gpu_pattern_type = 0;
        gpu_config.gpu_pattern_len = static_cast<uint32_t>(std::min<size_t>(pattern_arg.size(), gpu_config.gpu_pattern_chars.size()));
        std::copy_n(pattern_arg.begin(), gpu_config.gpu_pattern_len, gpu_config.gpu_pattern_chars.begin());
    } else if (pattern_type == "prefix") {
        gpu_config.gpu_pattern_type = 1;
        gpu_config.gpu_pattern_len = static_cast<uint32_t>(std::min<size_t>(pattern_arg.size(), gpu_config.gpu_pattern_chars.size()));
        std::copy_n(pattern_arg.begin(), gpu_config.gpu_pattern_len, gpu_config.gpu_pattern_chars.begin());
    } else if (pattern_type == "contains") {
        gpu_config.gpu_pattern_type = 2;
        gpu_config.gpu_pattern_len = static_cast<uint32_t>(std::min<size_t>(pattern_arg.size(), gpu_config.gpu_pattern_chars.size()));
        std::copy_n(pattern_arg.begin(), gpu_config.gpu_pattern_len, gpu_config.gpu_pattern_chars.begin());
    }

    // Setup generator
    std::unique_ptr<CPUGenerator> cpu_generator;
    std::unique_ptr<GPUGenerator> gpu_generator;

    if (use_gpu) {
        gpu_generator = std::make_unique<GPUGenerator>();
        gpu_generator->set_pattern(std::move(pattern));
        gpu_config.batch_size = gpu_batch_size;
        gpu_config.work_group_size = DEFAULT_WORK_GROUP_SIZE;
        gpu_config.num_batches = gpu_num_batches;
        gpu_config.verify_gpu_results = gpu_verify;
        gpu_config.profile = gpu_profile || benchmark_json;
        gpu_config.auto_tune_batch_size = gpu_auto_tune;
        gpu_config.auto_tune_batches = gpu_auto_tune_batches;
        gpu_config.auto_tune_candidates = gpu_auto_tune_candidates;
        gpu_config.verbose = verbose;
        gpu_generator->set_config(gpu_config);
        gpu_generator->initialize();
    } else {
        cpu_generator = std::make_unique<CPUGenerator>();
        cpu_generator->set_pattern(std::move(pattern));
        cpu_generator->set_num_threads(num_threads);
        cpu_generator->set_batch_size(1000);
        cpu_generator->set_max_attempts(max_attempts);
    }

    // Setup result callback
    std::ofstream out_file;
    if (!output_file.empty()) {
        out_file.open(output_file, std::ios::app);
    }

    auto result_callback = [&](const MatchResult& result) {
        std::cout << "\n╔══════════════════════════════════════════════════════════════╗\n";
        std::cout << "║  🎯 MATCH FOUND!                                             ║\n";
        std::cout << "╠══════════════════════════════════════════════════════════════╣\n";
        std::cout << "║  Address:    " << std::left << std::setw(46) << result.address << "║\n";
        std::cout << "║  Private Key: " << std::setw(46) << result.private_key_hex << "║\n";
        std::cout << "║  Attempts:   " << std::setw(46) << result.attempts << "║\n";
        std::cout << "╚══════════════════════════════════════════════════════════════╝\n";

        if (out_file.is_open()) {
            out_file << result.address << "," << result.private_key_hex << ","
                     << result.pattern_matched << "," << result.attempts << "\n";
            out_file.flush();
        }
    };

    if (use_gpu) {
        gpu_generator->set_callback(result_callback);
    } else {
        cpu_generator->set_callback(result_callback);
    }

    // Start generation
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    if (use_gpu) {
        gpu_generator->start();
    } else {
        cpu_generator->start();
    }

    auto start = std::chrono::steady_clock::now();
    auto last_update = start;
    uint64_t last_attempts = 0;

    while (g_running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        bool is_running = use_gpu ? gpu_generator->is_running() : cpu_generator->is_running();
        if (!is_running) break;

        auto now = std::chrono::steady_clock::now();
        if (verbose && std::chrono::duration<double>(now - last_update).count() >= 1.0) {
            uint64_t attempts = use_gpu ? gpu_generator->get_total_attempts() : cpu_generator->get_total_attempts();
            double elapsed = std::chrono::duration<double>(now - start).count();
            double rate = (attempts - last_attempts) /
                          std::chrono::duration<double>(now - last_update).count();
            double avg_rate = attempts / elapsed;
            size_t matches = use_gpu ? gpu_generator->get_results().size() : cpu_generator->get_results().size();

            std::cout << "\r[ " << std::fixed << std::setprecision(1) << elapsed << "s ] "
                      << "Attempts: " << std::setw(12) << attempts
                      << " | Rate: " << std::setw(10) << std::setprecision(0) << rate << " addr/s"
                      << " | Avg: " << std::setw(10) << avg_rate << " addr/s"
                      << " | Matches: " << matches
                      << "       " << std::flush;

            last_update = now;
            last_attempts = attempts;
        }
    }

    if (use_gpu) {
        gpu_generator->stop();
    } else {
        cpu_generator->stop();
    }

    auto end = std::chrono::steady_clock::now();
    double total_elapsed = std::chrono::duration<double>(end - start).count();
    uint64_t total_attempts = use_gpu ? gpu_generator->get_total_attempts() : cpu_generator->get_total_attempts();
    size_t total_matches = use_gpu ? gpu_generator->get_results().size() : cpu_generator->get_results().size();

    std::cout << "\n\n═══════════════════════════════════════════════════════════════\n";
    std::cout << "Generation Complete\n";
    std::cout << "Total Attempts: " << total_attempts << "\n";
    std::cout << "Total Time:     " << std::fixed << std::setprecision(2) << total_elapsed << "s\n";
    std::cout << "Average Rate:   " << std::setprecision(0) << (total_attempts / total_elapsed) << " addr/s\n";
    std::cout << "Matches Found:  " << total_matches << "\n";

    if (use_gpu && (gpu_profile || benchmark_json)) {
        GPUProfileStats stats = gpu_generator->get_profile_stats();
        if (gpu_profile) {
            std::cout << "\nGPU Profile\n";
            std::cout << "  Batches:             " << stats.batches << "\n";
            std::cout << "  GPU matches returned:" << stats.matches_returned << "\n";
            std::cout << std::fixed << std::setprecision(3);
            std::cout << "  Avg seed gen:        " << stats.avg(stats.seed_generation_ms) << " ms/batch\n";
            std::cout << "  Avg seed upload:     " << stats.avg(stats.seed_upload_ms) << " ms/batch\n";
            std::cout << "  Avg counter reset:   " << stats.avg(stats.counter_reset_ms) << " ms/batch\n";
            std::cout << "  Avg kernel:          " << stats.avg(stats.kernel_ms) << " ms/batch\n";
            std::cout << "  Avg count read:      " << stats.avg(stats.count_read_ms) << " ms/batch\n";
            std::cout << "  Avg result read:     " << stats.avg(stats.result_read_ms) << " ms/batch\n";
            std::cout << "  Avg host process:    " << stats.avg(stats.host_process_ms) << " ms/batch\n";
        }
        if (benchmark_json) {
            const size_t actual_gpu_batch_size = gpu_generator->get_batch_size();
            std::ostringstream json;
            json << std::fixed << std::setprecision(3)
                 << "{"
                 << "\"mode\":\"gpu\","
                 << "\"pattern_type\":\"" << pattern_type << "\","
                 << "\"pattern\":\"" << pattern_arg << "\","
                 << "\"batch_size\":" << actual_gpu_batch_size << ","
                 << "\"batches\":" << stats.batches << ","
                 << "\"attempts\":" << total_attempts << ","
                 << "\"elapsed_sec\":" << total_elapsed << ","
                 << "\"addr_per_sec\":" << (total_elapsed > 0.0 ? total_attempts / total_elapsed : 0.0) << ","
                 << "\"matches_found\":" << total_matches << ","
                 << "\"gpu_matches_returned\":" << stats.matches_returned << ","
                 << "\"avg_seed_generation_ms\":" << stats.avg(stats.seed_generation_ms) << ","
                 << "\"avg_seed_upload_ms\":" << stats.avg(stats.seed_upload_ms) << ","
                 << "\"avg_counter_reset_ms\":" << stats.avg(stats.counter_reset_ms) << ","
                 << "\"avg_kernel_ms\":" << stats.avg(stats.kernel_ms) << ","
                 << "\"avg_count_read_ms\":" << stats.avg(stats.count_read_ms) << ","
                 << "\"avg_result_read_ms\":" << stats.avg(stats.result_read_ms) << ","
                 << "\"avg_host_process_ms\":" << stats.avg(stats.host_process_ms)
                 << "}";
            std::cout << "\nBENCHMARK_JSON " << json.str() << "\n";
        }
    }

    std::cout << "═══════════════════════════════════════════════════════════════\n";

    return 0;
}

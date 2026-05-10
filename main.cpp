#include "utils/constants.h"
#include "utils/config.h"
#include "utils/crypto.h"
#include "utils/pattern.h"
#include "utils/estimate.h"
#include "utils/export_encryption.h"
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
#include <atomic>
#include <cctype>
#include <limits>
#include <cstdlib>

using namespace trx;

std::vector<size_t> parse_size_list(const std::string& csv) {
    std::vector<size_t> values;
    std::stringstream ss(csv);
    std::string item;
    while (std::getline(ss, item, ',')) {
        if (item.empty()) {
            throw std::invalid_argument("empty size item");
        }
        size_t pos = 0;
        unsigned long long value = std::stoull(item, &pos, 10);
        if (pos != item.size() || value == 0 || value > std::numeric_limits<size_t>::max()) {
            throw std::invalid_argument("invalid size item");
        }
        values.push_back(static_cast<size_t>(value));
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
║          Full GPU ECC + Base58 Filter (OpenCL)              ║
╚══════════════════════════════════════════════════════════════╝
)" << '\n';
}

void print_usage(const char* prog, std::ostream& os = std::cout) {
    os << "Usage:\n"
       << "  trx_vanity <pattern-type> <pattern> [options]\n"
       << "  " << prog << " <pattern-type> <pattern> [options]\n\n"
       << "Pattern types: prefix, suffix, contains, consecutive, sequential\n"
       << "  prefix <string>              Match after TRON's leading T, e.g. prefix ABC\n"
       << "  suffix <string>              Match address tail, e.g. suffix 8888\n"
       << "  contains <string>            Match anywhere, including TRON's fixed leading T, e.g. contains 520\n"
       << "  consecutive <char> [length]  Repeated suffix, e.g. consecutive 8 7 -> 8888888\n"
       << "  sequential <start> [length]  Ascending suffix, e.g. sequential 1 7 -> 1234567\n\n"
       << "Options:\n"
       << "  --gpu                 Use GPU acceleration (OpenCL)\n"
       << "  --batch-size, --gpu-batch <n> GPU addresses per batch (default: 65536)\n"
       << "  --device <platform:device> Select OpenCL platform/device indexes, 1-based (default: auto)\n"
       << "  --auto-tune           Benchmark candidate GPU batch sizes and use the fastest\n"
       << "  --auto-tune-sizes <csv> Candidate batch sizes (default: 32768,65536,131072,262144,524288)\n"
       << "  --auto-tune-batches <n> Batches per auto-tune candidate (default: 3)\n"
       << "  --batches <n>         GPU batch count, then stop (default: 0=infinite)\n"
       << "  --gpu-verify          Recompute matched GPU addresses on CPU for debugging\n"
       << "  --profile             Print GPU per-batch timing breakdown\n"
       << "  --benchmark-json      Print final benchmark metrics as JSON\n"
       << "  --max-attempts <n>    Stop after approximately n attempts (CPU smoke/CI)\n"
       << "  --show-private-key    Print matched private keys to stdout (unsafe; exposes funds)\n"
       << "  --encrypted-output <file>\n"
       << "                         Encrypted export including private keys (AES-256-GCM)\n"
       << "  --export-password <password>\n"
       << "                         Password for --encrypted-output (unsafe shell history/process-list risk)\n"
       << "  --export-password-env <name>\n"
       << "                         Read export password from environment variable <name>\n"
       << "  --allow-plaintext-private-key-output\n"
       << "                         Rejected: plaintext private-key file export is disabled for safety\n"
       << "  -t, --threads <n>     Number of CPU threads (default: auto)\n"
       << "  -o, --output <file>   Output file for matches; private keys are NOT written by default\n"
       << "  -v, --verbose         Show progress every second\n"
       << "  -h, --help            Show this help\n"
       << "  --config <file>       Load settings from JSON config file\n"
       << "  --save-config         Save current settings to default config file (~/.trx_vanity_config.json)\n\n"
       << "Examples:\n"
       << "  " << prog << " consecutive 8 7                 # 7 repeated 8s at the end\n"
       << "  " << prog << " consecutive 8 8                 # 8 repeated 8s at the end\n"
       << "  " << prog << " suffix 5201314 -t 8 -o results.txt\n"
       << "  " << prog << " suffix 8888 --max-attempts 100000 -t 4\n"
       << "  " << prog << " consecutive 8 7 --gpu --batch-size 65536\n";
}

bool is_supported_pattern_type(const std::string& type) {
    return type == "prefix" || type == "suffix" || type == "contains" ||
           type == "consecutive" || type == "sequential";
}

bool looks_like_option(const char* value) {
    return value && value[0] == '-';
}

bool is_valid_base58_string(const std::string& value) {
    return !value.empty() && std::all_of(value.begin(), value.end(), [](char c) {
        return std::string(BASE58_ALPHABET).find(c) != std::string::npos;
    });
}

PatternType cli_pattern_type_to_enum(const std::string& pattern_type) {
    if (pattern_type == "consecutive") return PatternType::SUFFIX_CONSECUTIVE;
    if (pattern_type == "sequential") return PatternType::SUFFIX_SEQUENTIAL;
    if (pattern_type == "suffix") return PatternType::SUFFIX_CUSTOM;
    if (pattern_type == "prefix") return PatternType::PREFIX_CUSTOM;
    return PatternType::CONTAINS;
}

size_t cli_pattern_length(const std::string& pattern_type,
                          const std::string& pattern_arg,
                          const std::string& pattern_arg2) {
    if (pattern_type == "consecutive" || pattern_type == "sequential") {
        return pattern_arg2.empty() ? 7 : static_cast<size_t>(std::stoul(pattern_arg2));
    }
    return pattern_arg.size();
}

int fail(const char* prog, const std::string& message, const std::string& hint = "", bool show_usage = false) {
    std::cerr << "Error: " << message << "\n";
    if (!hint.empty()) {
        std::cerr << "Hint: " << hint << "\n";
    }
    if (show_usage) {
        std::cerr << "\n";
        print_usage(prog, std::cerr);
    }
    return 2;
}

bool parse_positive_size(const std::string& text, size_t& out) {
    try {
        size_t pos = 0;
        unsigned long long value = std::stoull(text, &pos, 10);
        if (pos != text.size() || value == 0 || value > std::numeric_limits<size_t>::max()) return false;
        out = static_cast<size_t>(value);
        return true;
    } catch (...) {
        return false;
    }
}

bool parse_nonnegative_u64(const std::string& text, uint64_t& out) {
    try {
        size_t pos = 0;
        unsigned long long value = std::stoull(text, &pos, 10);
        if (pos != text.size()) return false;
        out = static_cast<uint64_t>(value);
        return true;
    } catch (...) {
        return false;
    }
}

int main(int argc, char* argv[]) {
    print_banner();

    // Config file support
    std::string config_file;
    bool save_config = false;
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--config") {
            if (i + 1 < argc) config_file = argv[++i];
        } else if (std::string(argv[i]) == "--save-config") {
            save_config = true;
        }
    }

    // Load config file if specified
    trx::CLIConfig config;
    if (!config_file.empty()) {
        std::string error;
        if (!trx::ConfigManager::load(config_file, config, error)) {
            std::cerr << "Warning: failed to load config: " << error << "\n";
        }
    }

    if (argc == 2 && (std::string(argv[1]) == "-h" || std::string(argv[1]) == "--help")) {
        print_usage(argv[0]);
        return 0;
    }
    if (argc < 2) {
        return fail(argv[0], "missing <pattern-type>.",
                    "Run 'trx_vanity --help' for examples and supported pattern types.", true);
    }
    if (argc < 3 || looks_like_option(argv[2])) {
        return fail(argv[0], "missing <pattern> for pattern type '" + std::string(argv[1]) + "'.",
                    "Command format: trx_vanity <pattern-type> <pattern> [options].", true);
    }

    // Parse arguments
    std::string pattern_type = argv[1];
    std::string pattern_arg = argv[2];
    std::string pattern_arg2;
    if (!is_supported_pattern_type(pattern_type)) {
        return fail(argv[0], "unknown pattern type '" + pattern_type + "'.",
                    "Supported pattern types: prefix, suffix, contains, consecutive, sequential.");
    }

    int num_threads = static_cast<int>(std::thread::hardware_concurrency());
    if (num_threads <= 0) num_threads = 1;
    std::string output_file;
    std::string encrypted_output_file;
    std::string export_password;
    std::string export_password_env;
    bool verbose = false;
    bool show_private_key = false;
    bool allow_plaintext_private_key_output = false;

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
    int gpu_platform_idx = -1;
    int gpu_device_idx = -1;

    // Apply config file defaults before CLI overrides
    if (config.gpu.has_value()) use_gpu = config.gpu.value();
    if (config.threads.has_value()) num_threads = config.threads.value();
    if (config.batch_size.has_value()) gpu_batch_size = config.batch_size.value();
    if (config.max_attempts.has_value()) max_attempts = config.max_attempts.value();
    if (config.verbose.has_value()) verbose = config.verbose.value();
    if (config.output.has_value()) output_file = config.output.value();
    if (config.encrypted_output.has_value()) encrypted_output_file = config.encrypted_output.value();
    if (config.export_password_env.has_value()) export_password_env = config.export_password_env.value();
    if (config.show_private_key.has_value()) show_private_key = config.show_private_key.value();

    auto require_value = [&](int index, const std::string& opt) -> bool {
        if (index + 1 >= argc || looks_like_option(argv[index + 1])) {
            fail(argv[0], opt + " requires a value.", "Run 'trx_vanity --help' to see option syntax.");
            return false;
        }
        return true;
    };

    for (int i = 3; i < argc; ++i) {
        std::string arg = argv[i];
        if ((arg == "-t" || arg == "--threads")) {
            if (!require_value(i, arg)) return 2;
            size_t parsed = 0;
            if (!parse_positive_size(argv[++i], parsed) || parsed > static_cast<size_t>(std::numeric_limits<int>::max())) {
                return fail(argv[0], arg + " must be a positive integer.", "Example: --threads 4");
            }
            num_threads = static_cast<int>(parsed);
        } else if ((arg == "-o" || arg == "--output")) {
            if (!require_value(i, arg)) return 2;
            output_file = argv[++i];
        } else if (arg == "--encrypted-output") {
            if (!require_value(i, arg)) return 2;
            encrypted_output_file = argv[++i];
        } else if (arg == "--export-password") {
            if (!require_value(i, arg)) return 2;
            export_password = argv[++i];
        } else if (arg == "--export-password-env") {
            if (!require_value(i, arg)) return 2;
            export_password_env = argv[++i];
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
        } else if (arg == "--show-private-key") {
            show_private_key = true;
        } else if (arg == "--allow-plaintext-private-key-output") {
            allow_plaintext_private_key_output = true;
        } else if (arg == "--auto-tune") {
            use_gpu = true;
            gpu_auto_tune = true;
        } else if (arg == "--auto-tune-batches") {
            if (!require_value(i, arg)) return 2;
            if (!parse_positive_size(argv[++i], gpu_auto_tune_batches)) {
                return fail(argv[0], arg + " must be a positive integer.", "Example: --auto-tune-batches 3");
            }
        } else if (arg == "--auto-tune-sizes") {
            if (!require_value(i, arg)) return 2;
            try {
                gpu_auto_tune_candidates = parse_size_list(argv[++i]);
            } catch (...) {
                return fail(argv[0], arg + " must be a comma-separated list of positive integers.", "Example: --auto-tune-sizes 32768,65536");
            }
            if (gpu_auto_tune_candidates.empty() || std::any_of(gpu_auto_tune_candidates.begin(), gpu_auto_tune_candidates.end(), [](size_t n) { return n == 0; })) {
                return fail(argv[0], arg + " must include at least one positive integer.", "Example: --auto-tune-sizes 32768,65536");
            }
        } else if (arg == "--batch-size" || arg == "--gpu-batch") {
            if (!require_value(i, arg)) return 2;
            if (!parse_positive_size(argv[++i], gpu_batch_size)) {
                return fail(argv[0], arg + " must be a positive integer.", "Example: --batch-size 65536");
            }
        } else if (arg == "--batches") {
            if (!require_value(i, arg)) return 2;
            uint64_t parsed = 0;
            if (!parse_nonnegative_u64(argv[++i], parsed) || parsed > std::numeric_limits<size_t>::max()) {
                return fail(argv[0], arg + " must be a non-negative integer.", "Use --batches 0 for infinite GPU generation.");
            }
            gpu_num_batches = static_cast<size_t>(parsed);
        } else if (arg == "--max-attempts") {
            if (!require_value(i, arg)) return 2;
            if (!parse_nonnegative_u64(argv[++i], max_attempts)) {
                return fail(argv[0], arg + " must be a non-negative integer.", "Example: --max-attempts 1000000");
            }
        } else if (arg == "--device") {
            if (!require_value(i, arg)) return 2;
            std::string spec = argv[++i];
            size_t colon = spec.find(':');
            size_t platform = 0;
            size_t device = 0;
            if (colon == std::string::npos || !parse_positive_size(spec.substr(0, colon), platform) || !parse_positive_size(spec.substr(colon + 1), device)) {
                return fail(argv[0], arg + " must use <platform:device> with positive integer indexes.", "Example: --device 1:1");
            }
            gpu_platform_idx = static_cast<int>(platform - 1);
            gpu_device_idx = static_cast<int>(device - 1);
        } else if (arg == "--config") {
            // Already parsed above; skip value
            if (i + 1 < argc && !looks_like_option(argv[i + 1])) ++i;
        } else if (arg == "--save-config") {
            // Already parsed above; no action needed
        } else if (i == 3 && (pattern_type == "consecutive" || pattern_type == "sequential")) {
            pattern_arg2 = arg;
        } else {
            return fail(argv[0], "unknown option or unexpected argument '" + arg + "'.",
                        "Run 'trx_vanity --help' to see supported options.");
        }
    }

    if (allow_plaintext_private_key_output) {
        return fail(argv[0], "plaintext private-key file export is disabled.",
                    "Use --encrypted-output <file> --export-password-env <name> to export private keys safely.");
    }
    if (!export_password.empty() && !export_password_env.empty()) {
        return fail(argv[0], "use only one export password source.",
                    "Choose either --export-password-env <name> or --export-password <password>, not both.");
    }
    if (!export_password_env.empty()) {
        const char* env_password = std::getenv(export_password_env.c_str());
        if (env_password == nullptr || env_password[0] == '\0') {
            return fail(argv[0], "environment variable for --export-password-env is not set or is empty.",
                        "Set the named variable to a strong export password before running the command.");
        }
        export_password = env_password;
    }
    if (!encrypted_output_file.empty() && export_password.empty()) {
        return fail(argv[0], "--encrypted-output requires an export password.",
                    "Prefer --export-password-env <name>; --export-password <password> is available but may be exposed in shell history/process lists.");
    }
    if (encrypted_output_file.empty() && !export_password.empty()) {
        return fail(argv[0], "an export password was provided without --encrypted-output.",
                    "Passwords are only used for encrypted private-key export.");
    }

    auto validate_pattern = [&]() -> int {
        if (pattern_arg.empty()) {
            return fail(argv[0], "pattern must not be empty.", "Provide a Base58 pattern such as suffix 8888.");
        }
        if (pattern_type == "prefix" || pattern_type == "suffix" || pattern_type == "contains") {
            if (!is_valid_base58_string(pattern_arg)) {
                return fail(argv[0], "pattern contains characters that are not valid Base58.",
                            "Base58 excludes visually ambiguous characters: 0, O, I, l.");
            }
            if (pattern_type == "prefix" && pattern_arg.size() > 10) {
                return fail(argv[0], "prefix pattern is too long.", "Prefix length must be 1-10 characters.");
            }
            if ((pattern_type == "suffix" || pattern_type == "contains") && pattern_arg.size() > 20) {
                return fail(argv[0], "pattern is too long.", "Suffix/contains length must be 1-20 characters.");
            }
        } else if (pattern_type == "consecutive") {
            if (pattern_arg.size() != 1 || !is_valid_base58_string(pattern_arg) || !std::isdigit(static_cast<unsigned char>(pattern_arg[0]))) {
                return fail(argv[0], "consecutive pattern must be a single Base58 character currently supported as digit 1-9.", "Example: consecutive 8 7");
            }
            size_t length = 0;
            if (!pattern_arg2.empty() && !parse_positive_size(pattern_arg2, length)) {
                return fail(argv[0], "consecutive length must be a positive integer.", "Example: consecutive 8 7");
            }
            length = pattern_arg2.empty() ? 7 : length;
            if (length > 20) {
                return fail(argv[0], "consecutive length is too long.", "Length must be 1-20.");
            }
        } else if (pattern_type == "sequential") {
            if (pattern_arg.size() != 1 || !std::isdigit(static_cast<unsigned char>(pattern_arg[0]))) {
                return fail(argv[0], "sequential pattern must start with a single digit.", "Example: sequential 1 7");
            }
            size_t length = 0;
            if (!pattern_arg2.empty() && !parse_positive_size(pattern_arg2, length)) {
                return fail(argv[0], "sequential length must be a positive integer.", "Example: sequential 1 7");
            }
            length = pattern_arg2.empty() ? 7 : length;
            if (length > 10) {
                return fail(argv[0], "sequential length is too long.", "Length must be 1-10.");
            }
            if ((pattern_arg[0] - '0') + static_cast<int>(length) - 1 > 9) {
                return fail(argv[0], "sequential pattern cannot produce a digit-only ascending suffix with this start and length.",
                            "Example: sequential 1 7 produces 1234567; sequential 9 4 is invalid.");
            }
        }
        return 0;
    };
    if (int validation_result = validate_pattern(); validation_result != 0) return validation_result;

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
            return fail(argv[0], "unknown pattern type '" + pattern_type + "'.",
                        "Supported pattern types: prefix, suffix, contains, consecutive, sequential.");
        }
    } catch (const std::exception& e) {
        return fail(argv[0], std::string("could not create pattern: ") + e.what(),
                    "Run 'trx_vanity --help' to see valid pattern forms.");
    }

    std::cout << "Pattern: " << pattern->description() << "\n";
    PatternEstimate pattern_estimate = estimate_pattern(
        cli_pattern_type_to_enum(pattern_type),
        pattern_arg,
        cli_pattern_length(pattern_type, pattern_arg, pattern_arg2));
    std::cout << format_estimate_summary(pattern_estimate) << "\n";
    std::cout << "Mode:    " << (use_gpu ? "GPU (OpenCL)" : "CPU") << "\n";
    if (!use_gpu) {
        std::cout << "Threads: " << num_threads << "\n";
    }
    const std::string output_label = !encrypted_output_file.empty()
        ? (output_file.empty() ? (std::string("encrypted:") + encrypted_output_file)
                               : (output_file + " + encrypted:" + encrypted_output_file))
        : (output_file.empty() ? "stdout" : output_file);
    std::cout << "Output:  " << output_label << "\n\n";
    if (show_private_key) {
        std::cerr << "WARNING: --show-private-key will print private keys to stdout. "
                  << "Anyone with this output can spend funds sent to the matched address.\n";
    }
    if (allow_plaintext_private_key_output) {
        std::cerr << "WARNING: --allow-plaintext-private-key-output is disabled and cannot write private keys.\n";
    } else if (!output_file.empty()) {
        std::cout << "Security: private keys are NOT written to the plaintext output file. "
                  << "Use --encrypted-output with --export-password-env to export private keys.\n\n";
    }
    if (!encrypted_output_file.empty()) {
        std::cout << "Security: encrypted export enabled (AES-256-GCM). The password is not printed or logged.\n\n";
    }

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
#ifdef USE_OPENCL
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
        gpu_config.platform_idx = gpu_platform_idx;
        gpu_config.device_idx = gpu_device_idx;
        gpu_generator->set_config(gpu_config);
        try {
            gpu_generator->initialize();
        } catch (const std::exception& e) {
            return fail(argv[0], std::string("GPU initialization failed: ") + e.what(),
                        "Check that OpenCL is available and that --device uses valid 1-based platform:device indexes, or omit --device for auto selection.");
        }
    } else {
        cpu_generator = std::make_unique<CPUGenerator>();
        cpu_generator->set_pattern(std::move(pattern));
        cpu_generator->set_num_threads(num_threads);
        cpu_generator->set_batch_size(1000);
        cpu_generator->set_max_attempts(max_attempts);
    }
#else
    cpu_generator = std::make_unique<CPUGenerator>();
    cpu_generator->set_pattern(std::move(pattern));
    cpu_generator->set_num_threads(num_threads);
    cpu_generator->set_batch_size(1000);
    cpu_generator->set_max_attempts(max_attempts);
#endif

    // Setup result callback
    std::ofstream out_file;
    if (!output_file.empty()) {
        out_file.open(output_file, std::ios::app);
        if (!out_file.is_open()) {
            return fail(argv[0], "could not open plaintext output file for writing.",
                        "Check the path and permissions.");
        }
    }
    std::ofstream encrypted_out_file;
    if (!encrypted_output_file.empty()) {
        encrypted_out_file.open(encrypted_output_file, std::ios::app);
        if (!encrypted_out_file.is_open()) {
            return fail(argv[0], "could not open encrypted output file for writing.",
                        "Check the path and permissions.");
        }
    }

    auto result_callback = [&](const MatchResult& result) {
        std::cout << "\n╔══════════════════════════════════════════════════════════════╗\n";
        std::cout << "║  🎯 MATCH FOUND!                                             ║\n";
        std::cout << "╠══════════════════════════════════════════════════════════════╣\n";
        std::cout << "║  Address:    " << std::left << std::setw(46) << result.address << "║\n";
        std::cout << "║  Rule:       " << std::setw(46) << result.pattern_matched << "║\n";
        std::cout << "║  Attempts:   " << std::setw(46) << result.attempts << "║\n";
        if (show_private_key) {
            std::cout << "║  WARNING: private key shown; keep it secret.          ║\n";
            std::cout << "║  Private Key: " << std::setw(46) << result.private_key_hex << "║\n";
        } else {
            std::cout << "║  Security: private key hidden by default.             ║\n";
        }
        std::cout << "╚══════════════════════════════════════════════════════════════╝\n";

        if (out_file.is_open()) {
            out_file << result.address << "," << result.pattern_matched << ","
                     << result.attempts << ",private_key_hidden\n";
            out_file.flush();
        }
        if (encrypted_out_file.is_open()) {
            const std::string plaintext_record = result.address + "," + result.private_key_hex + "," +
                                                 result.pattern_matched + "," + std::to_string(result.attempts);
            try {
                encrypted_out_file << encrypt_export_record(plaintext_record, export_password) << "\n";
                encrypted_out_file.flush();
            } catch (const std::exception& e) {
                std::cerr << "Error: encrypted export failed: " << e.what() << "\n";
                g_running = false;
            }
        }
    };

#ifdef USE_OPENCL
    if (use_gpu) {
        gpu_generator->set_callback(result_callback);
    } else {
        cpu_generator->set_callback(result_callback);
    }
#else
    cpu_generator->set_callback(result_callback);
#endif

    // Start generation
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

#ifdef USE_OPENCL
    if (use_gpu) {
        gpu_generator->start();
    } else {
        cpu_generator->start();
    }
#else
    cpu_generator->start();
#endif

    auto start = std::chrono::steady_clock::now();
    auto last_update = start;
    uint64_t last_attempts = 0;

    while (g_running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

#ifdef USE_OPENCL
        bool is_running = use_gpu ? gpu_generator->is_running() : cpu_generator->is_running();
#else
        bool is_running = cpu_generator->is_running();
#endif
        if (!is_running) break;

        auto now = std::chrono::steady_clock::now();
        if (verbose && std::chrono::duration<double>(now - last_update).count() >= 1.0) {
#ifdef USE_OPENCL
            uint64_t attempts = use_gpu ? gpu_generator->get_total_attempts() : cpu_generator->get_total_attempts();
#else
            uint64_t attempts = cpu_generator->get_total_attempts();
#endif
            double elapsed = std::chrono::duration<double>(now - start).count();
            double rate = (attempts - last_attempts) /
                          std::chrono::duration<double>(now - last_update).count();
            double avg_rate = attempts / elapsed;
#ifdef USE_OPENCL
            size_t matches = use_gpu ? gpu_generator->get_results().size() : cpu_generator->get_results().size();
#else
            size_t matches = cpu_generator->get_results().size();
#endif

            RuntimeEstimate runtime_estimate = estimate_runtime(pattern_estimate, avg_rate, attempts);
            std::cout << "\r[ " << std::fixed << std::setprecision(1) << elapsed << "s ] "
                      << "Attempts: " << std::setw(12) << attempts
                      << " | Rate: " << std::setw(10) << std::setprecision(0) << rate << " addr/s"
                      << " | Avg: " << std::setw(10) << avg_rate << " addr/s"
                      << " | ETA(avg): " << std::setw(11) << format_duration(runtime_estimate.average_seconds)
                      << " | Prob: " << std::setw(7) << std::setprecision(4) << (runtime_estimate.probability_progress * 100.0) << "%"
                      << " | Matches: " << matches
                      << "       " << std::flush;

            last_update = now;
            last_attempts = attempts;
        }
    }

#ifdef USE_OPENCL
    if (use_gpu) {
        gpu_generator->stop();
    } else {
        cpu_generator->stop();
    }

    auto end = std::chrono::steady_clock::now();
    double total_elapsed = std::chrono::duration<double>(end - start).count();
    uint64_t total_attempts = use_gpu ? gpu_generator->get_total_attempts() : cpu_generator->get_total_attempts();
    size_t total_matches = use_gpu ? gpu_generator->get_results().size() : cpu_generator->get_results().size();
#else
    cpu_generator->stop();

    auto end = std::chrono::steady_clock::now();
    double total_elapsed = std::chrono::duration<double>(end - start).count();
    uint64_t total_attempts = cpu_generator->get_total_attempts();
    size_t total_matches = cpu_generator->get_results().size();
#endif

    std::cout << "\n\n═══════════════════════════════════════════════════════════════\n";
    std::cout << "Generation Complete\n";
    std::cout << "Total Attempts: " << total_attempts << "\n";
    std::cout << "Total Time:     " << std::fixed << std::setprecision(2) << total_elapsed << "s\n";
    std::cout << "Average Rate:   " << std::setprecision(0) << (total_attempts / total_elapsed) << " addr/s\n";
    std::cout << "Matches Found:  " << total_matches << "\n";

#ifdef USE_OPENCL
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
                 << "\"avg_host_process_ms\":\"" << stats.avg(stats.host_process_ms)
                 << "}";
            std::cout << "\nBENCHMARK_JSON " << json.str() << "\n";
        }
    }
#endif

    // Save config if requested
    if (save_config) {
        std::string save_path = config_file.empty() ? trx::ConfigManager::default_config_path() : config_file;
        trx::CLIConfig save_config;
        save_config.gpu = use_gpu;
        save_config.threads = num_threads;
        save_config.batch_size = gpu_batch_size;
        save_config.max_attempts = max_attempts;
        save_config.verbose = verbose;
        save_config.show_private_key = show_private_key;
        if (!output_file.empty()) save_config.output = output_file;
        if (!encrypted_output_file.empty()) save_config.encrypted_output = encrypted_output_file;
        if (!export_password_env.empty()) save_config.export_password_env = export_password_env;

        std::string error;
        if (trx::ConfigManager::save(save_path, save_config, error)) {
            std::cout << "\nConfig saved to: " << save_path << "\n";
        } else {
            std::cerr << "\nWarning: failed to save config: " << error << "\n";
        }
    }

    std::cout << "═══════════════════════════════════════════════════════════════\n";

    return 0;
}

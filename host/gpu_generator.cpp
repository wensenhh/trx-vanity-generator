#include "gpu_generator.h"
#include "utils/crypto.h"
#include "utils/rng.h"
#include <iostream>
#include <fstream>
#include <chrono>
#include <cstring>
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <stdexcept>
#include <limits>
#include <iomanip>

namespace trx {

namespace {

double elapsed_ms(std::chrono::steady_clock::time_point start,
                  std::chrono::steady_clock::time_point end) {
    return std::chrono::duration<double, std::milli>(end - start).count();
}

std::string find_vanity_kernel_path() {
    namespace fs = std::filesystem;

    std::vector<fs::path> candidates;
    if (const char* env_dir = std::getenv("TRX_KERNEL_DIR")) {
        candidates.emplace_back(fs::path(env_dir) / "vanity.cl");
    }

    const fs::path cwd = fs::current_path();
    candidates.emplace_back(cwd / "kernel" / "vanity.cl"); // source-tree execution
    candidates.emplace_back(cwd / "vanity.cl");              // build-dir execution
    candidates.emplace_back(cwd / ".." / "kernel" / "vanity.cl");

    for (const auto& candidate : candidates) {
        std::error_code ec;
        if (fs::exists(candidate, ec)) {
            return fs::canonical(candidate, ec).string();
        }
    }

    throw std::runtime_error(
        "Unable to locate OpenCL kernel vanity.cl. Run from the project root/build "
        "directory, or set TRX_KERNEL_DIR to the kernel directory."
    );
}

} // namespace

GPUGenerator::GPUGenerator()
    : kernel_(nullptr)
    , seeds_buffer_(nullptr)
    , results_buffer_(nullptr)
    , addresses_buffer_(nullptr)
    , match_count_buffer_(nullptr)
    , pattern_buffer_(nullptr) {}

GPUGenerator::~GPUGenerator() {
    stop();
    if (cl_) {
        cl_->release_buffer(seeds_buffer_);
        cl_->release_buffer(results_buffer_);
        cl_->release_buffer(addresses_buffer_);
        cl_->release_buffer(match_count_buffer_);
        cl_->release_buffer(pattern_buffer_);
    }
}

void GPUGenerator::set_config(const GPUGenerationConfig& config) {
    config_ = config;
}

void GPUGenerator::set_pattern(std::unique_ptr<Pattern> pattern) {
    matcher_ = MultiPatternMatcher();
    matcher_.add_pattern(std::move(pattern));
}

void GPUGenerator::add_pattern(std::unique_ptr<Pattern> pattern) {
    matcher_.add_pattern(std::move(pattern));
}

void GPUGenerator::set_callback(ResultCallback cb) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    callback_ = cb;
}

size_t GPUGenerator::auto_tune_batch_size() {
    std::vector<size_t> candidates = config_.auto_tune_candidates;
    candidates.erase(
        std::remove_if(candidates.begin(), candidates.end(), [](size_t n) { return n == 0; }),
        candidates.end()
    );
    std::sort(candidates.begin(), candidates.end());
    candidates.erase(std::unique(candidates.begin(), candidates.end()), candidates.end());

    if (candidates.empty()) {
        return config_.batch_size == 0 ? 65536 : config_.batch_size;
    }

    const size_t batches = std::max<size_t>(1, config_.auto_tune_batches);
    const size_t local_size = std::max<size_t>(1, config_.work_group_size);
    RNG rng;

    std::cout << "GPU batch auto-tune: " << candidates.size()
              << " candidate(s), " << batches << " batch(es) each\n";

    size_t best_batch_size = candidates.front();
    double best_rate = -std::numeric_limits<double>::infinity();

    for (size_t candidate : candidates) {
        std::vector<cl_uint4> seeds(candidate);
        const size_t seeds_size = candidate * sizeof(cl_uint4);
        const size_t results_size = candidate * sizeof(GPUMatchResult);
        const size_t addresses_size = candidate * TRX_ADDRESS_SIZE * sizeof(cl_uchar);
        const size_t match_count_size = sizeof(cl_uint);
        const size_t pattern_size = config_.gpu_pattern_chars.size() * sizeof(cl_uchar);

        cl_mem seeds_buffer = nullptr;
        cl_mem results_buffer = nullptr;
        cl_mem addresses_buffer = nullptr;
        cl_mem match_count_buffer = nullptr;
        cl_mem pattern_buffer = nullptr;

        double total_ms = 0.0;
        uint64_t matches_returned = 0;

        try {
            seeds_buffer = cl_->create_buffer(CL_MEM_READ_ONLY, seeds_size);
            results_buffer = cl_->create_buffer(CL_MEM_WRITE_ONLY, results_size);
            addresses_buffer = cl_->create_buffer(CL_MEM_WRITE_ONLY, addresses_size);
            match_count_buffer = cl_->create_buffer(CL_MEM_READ_WRITE, match_count_size);
            pattern_buffer = cl_->create_buffer(CL_MEM_READ_ONLY, pattern_size);

            cl_->write_buffer(pattern_buffer, pattern_size, config_.gpu_pattern_chars.data(), true);

            cl_uint batch_size_arg = static_cast<cl_uint>(candidate);
            cl_uint pattern_type_arg = static_cast<cl_uint>(config_.gpu_pattern_type);
            cl_uint pattern_len_arg = static_cast<cl_uint>(config_.gpu_pattern_len);
            cl_->set_kernel_arg_buffer(kernel_, 0, seeds_buffer);
            cl_->set_kernel_arg_buffer(kernel_, 1, results_buffer);
            cl_->set_kernel_arg_buffer(kernel_, 2, match_count_buffer);
            cl_->set_kernel_arg_buffer(kernel_, 3, addresses_buffer);
            cl_->set_kernel_arg(kernel_, 4, sizeof(cl_uint), &batch_size_arg);
            cl_->set_kernel_arg(kernel_, 5, sizeof(cl_uint), &pattern_type_arg);
            cl_->set_kernel_arg(kernel_, 6, sizeof(cl_uint), &pattern_len_arg);
            cl_->set_kernel_arg_buffer(kernel_, 7, pattern_buffer);

            size_t global_size = candidate;
            if (global_size % local_size != 0) {
                global_size = ((global_size / local_size) + 1) * local_size;
            }

            for (size_t batch = 0; batch < batches; ++batch) {
                auto batch_start = std::chrono::steady_clock::now();

                for (size_t i = 0; i < candidate; ++i) {
                    auto seed = rng.generate_seed();
                    seeds[i].s[0] = seed[0];
                    seeds[i].s[1] = seed[1];
                    seeds[i].s[2] = seed[2];
                    seeds[i].s[3] = seed[3];
                }

                // Queue seed upload + counter reset asynchronously. The in-order
                // command queue preserves upload -> reset -> kernel -> readback,
                // while the final blocking read below is the only host sync point.
                cl_->write_buffer(seeds_buffer, seeds_size, seeds.data(), false);
                cl_uint zero = 0;
                cl_->fill_buffer(match_count_buffer, &zero, sizeof(cl_uint), sizeof(cl_uint), false);
                cl_->enqueue_nd_range_timed_ms(kernel_, 1, &global_size, &local_size);

                cl_uint match_count = 0;
                cl_->read_buffer(match_count_buffer, sizeof(cl_uint), &match_count, true);
                matches_returned += std::min<uint64_t>(match_count, static_cast<uint64_t>(candidate));

                auto batch_end = std::chrono::steady_clock::now();
                total_ms += elapsed_ms(batch_start, batch_end);
            }
        } catch (...) {
            cl_->release_buffer(seeds_buffer);
            cl_->release_buffer(results_buffer);
            cl_->release_buffer(addresses_buffer);
            cl_->release_buffer(match_count_buffer);
            cl_->release_buffer(pattern_buffer);
            throw;
        }

        cl_->release_buffer(seeds_buffer);
        cl_->release_buffer(results_buffer);
        cl_->release_buffer(addresses_buffer);
        cl_->release_buffer(match_count_buffer);
        cl_->release_buffer(pattern_buffer);

        const double attempts = static_cast<double>(candidate * batches);
        const double seconds = total_ms / 1000.0;
        const double rate = seconds > 0.0 ? attempts / seconds : 0.0;
        std::cout << "  batch_size=" << candidate
                  << " rate=" << std::fixed << std::setprecision(0) << rate << " addr/s"
                  << " avg_batch_ms=" << std::setprecision(3) << (total_ms / static_cast<double>(batches))
                  << " gpu_matches=" << matches_returned << "\n";

        if (rate > best_rate) {
            best_rate = rate;
            best_batch_size = candidate;
        }
    }

    std::cout << "Auto-tune selected batch_size=" << best_batch_size
              << " (" << std::fixed << std::setprecision(0) << best_rate << " addr/s)\n\n";
    return best_batch_size;
}

void GPUGenerator::initialize() {
    cl_ = std::make_unique<OpenCLManager>();
    cl_->initialize();

    auto device = cl_->get_selected_device();
    std::cout << "GPU Device: " << device.name << "\n";
    std::cout << "  Vendor: " << device.vendor << "\n";
    std::cout << "  Compute Units: " << device.compute_units << "\n";
    std::cout << "  Max Work Group: " << device.max_work_group_size << "\n";
    std::cout << "  Global Memory: " << (device.global_mem_size / (1024*1024)) << " MB\n\n";

    if (config_.work_group_size == 0) {
        config_.work_group_size = std::min<size_t>(256, device.max_work_group_size);
    }
    if (config_.batch_size == 0) {
        config_.batch_size = 65536;
    }

    std::cout << "Config: batch_size=" << config_.batch_size
              << ", work_group_size=" << config_.work_group_size << "\n\n";

    // Load Phase 3 full GPU kernel. The path is resolved at runtime so the
    // binary works from the source tree, build directory, install location with
    // TRX_KERNEL_DIR, and tests/CI workdirs.
    const std::string kernel_path = find_vanity_kernel_path();
    cl_->load_kernel("generate_addresses_full_gpu", kernel_path);
    cl_->build_program("-cl-std=CL1.2 -Werror -DUSE_OPENCL=1");

    kernel_ = cl_->get_kernel("generate_addresses_full_gpu");

    if (config_.auto_tune_batch_size) {
        config_.batch_size = auto_tune_batch_size();
    }

    // Create buffers. The current full-GPU kernel returns every generated
    // address for CPU-side Base58 verification (no GPU prefilter yet), so the
    // result buffers must be able to hold the whole batch. A smaller fixed cap
    // silently drops candidates and creates false negatives.
    size_t seeds_size = config_.batch_size * sizeof(cl_uint4);
    size_t results_size = config_.batch_size * sizeof(GPUMatchResult);
    size_t addresses_size = config_.batch_size * TRX_ADDRESS_SIZE * sizeof(cl_uchar);
    size_t match_count_size = sizeof(cl_uint);
    size_t pattern_size = config_.gpu_pattern_chars.size() * sizeof(cl_uchar);

    seeds_buffer_ = cl_->create_buffer(CL_MEM_READ_ONLY, seeds_size);
    results_buffer_ = cl_->create_buffer(CL_MEM_WRITE_ONLY, results_size);
    addresses_buffer_ = cl_->create_buffer(CL_MEM_WRITE_ONLY, addresses_size);
    match_count_buffer_ = cl_->create_buffer(CL_MEM_READ_WRITE, match_count_size);
    pattern_buffer_ = cl_->create_buffer(CL_MEM_READ_ONLY, pattern_size);

    cl_->write_buffer(pattern_buffer_, pattern_size, config_.gpu_pattern_chars.data(), true);

    // Set kernel args (0=seeds, 1=results, 2=match_count, 3=addresses_out,
    // 4=batch_size, 5=pattern_type, 6=pattern_len, 7=pattern_chars)
    cl_uint batch_size_arg = static_cast<cl_uint>(config_.batch_size);
    cl_uint pattern_type_arg = static_cast<cl_uint>(config_.gpu_pattern_type);
    cl_uint pattern_len_arg = static_cast<cl_uint>(config_.gpu_pattern_len);
    cl_->set_kernel_arg_buffer(kernel_, 0, seeds_buffer_);
    cl_->set_kernel_arg_buffer(kernel_, 1, results_buffer_);
    cl_->set_kernel_arg_buffer(kernel_, 2, match_count_buffer_);
    cl_->set_kernel_arg_buffer(kernel_, 3, addresses_buffer_);
    cl_->set_kernel_arg(kernel_, 4, sizeof(cl_uint), &batch_size_arg);
    cl_->set_kernel_arg(kernel_, 5, sizeof(cl_uint), &pattern_type_arg);
    cl_->set_kernel_arg(kernel_, 6, sizeof(cl_uint), &pattern_len_arg);
    cl_->set_kernel_arg_buffer(kernel_, 7, pattern_buffer_);
}

void GPUGenerator::upload_pattern_data() {
    // Upload pattern matching parameters to GPU
    // For now, simplified - full pattern matching done on CPU for candidates
    // Phase 3 will implement full GPU pattern matching
}

void GPUGenerator::start() {
    if (running_.load()) return;

    running_ = true;
    stop_requested_ = false;
    total_attempts_ = 0;
    start_time_ = std::chrono::steady_clock::now();
    results_.clear();
    {
        std::lock_guard<std::mutex> lock(profile_mutex_);
        profile_stats_ = GPUProfileStats{};
    }

    generation_thread_ = std::thread(&GPUGenerator::generation_loop, this);
}

void GPUGenerator::stop() {
    stop_requested_ = true;
    running_ = false;

    if (generation_thread_.joinable()) {
        generation_thread_.join();
    }
}

void GPUGenerator::generation_loop() {
    RNG rng;
    std::vector<cl_uint4> seeds(config_.batch_size);
    std::vector<GPUMatchResult> gpu_results(config_.batch_size);
    std::vector<cl_uchar> gpu_addresses(config_.batch_size * TRX_ADDRESS_SIZE);

    while (!stop_requested_.load()) {
        GPUProfileStats batch_stats;
        auto step_start = std::chrono::steady_clock::now();

        // Generate seeds for this batch
        for (size_t i = 0; i < config_.batch_size; ++i) {
            auto seed = rng.generate_seed();
            seeds[i].s[0] = seed[0];
            seeds[i].s[1] = seed[1];
            seeds[i].s[2] = seed[2];
            seeds[i].s[3] = seed[3];
        }
        auto step_end = std::chrono::steady_clock::now();
        batch_stats.seed_generation_ms = elapsed_ms(step_start, step_end);

        // Upload seeds. Keep this non-blocking; this queue is in-order, so the
        // subsequent counter reset, kernel launch, and blocking readback provide
        // correct ordering with one synchronization point per batch.
        step_start = std::chrono::steady_clock::now();
        cl_->write_buffer(seeds_buffer_, seeds.size() * sizeof(cl_uint4), seeds.data(), false);
        step_end = std::chrono::steady_clock::now();
        batch_stats.seed_upload_ms = elapsed_ms(step_start, step_end);

        // Reset match count on-device instead of doing a blocking 4-byte host
        // write. This removes a tiny-but-costly sync from every no-match batch.
        cl_uint zero = 0;
        step_start = std::chrono::steady_clock::now();
        cl_->fill_buffer(match_count_buffer_, &zero, sizeof(cl_uint), sizeof(cl_uint), false);
        step_end = std::chrono::steady_clock::now();
        batch_stats.counter_reset_ms = elapsed_ms(step_start, step_end);

        // Launch kernel
        size_t global_size = config_.batch_size;
        size_t local_size = config_.work_group_size;

        // Round up to multiple of work group size
        if (global_size % local_size != 0) {
            global_size = ((global_size / local_size) + 1) * local_size;
        }

        if (config_.profile) {
            batch_stats.kernel_ms = cl_->enqueue_nd_range_timed_ms(kernel_, 1, &global_size, &local_size);
        } else {
            step_start = std::chrono::steady_clock::now();
            cl_->enqueue_nd_range(kernel_, 1, &global_size, &local_size);
            step_end = std::chrono::steady_clock::now();
            batch_stats.kernel_ms = elapsed_ms(step_start, step_end);
        }

        // Read results
        cl_uint match_count = 0;
        step_start = std::chrono::steady_clock::now();
        cl_->read_buffer(match_count_buffer_, sizeof(cl_uint), &match_count, true);
        step_end = std::chrono::steady_clock::now();
        batch_stats.count_read_ms = elapsed_ms(step_start, step_end);

        if (match_count > 0) {
            if (match_count > config_.batch_size) {
                std::cerr << "WARNING: GPU returned match_count=" << match_count
                          << " greater than batch_size=" << config_.batch_size
                          << "; clamping readback to allocated buffer size\n";
                match_count = static_cast<cl_uint>(config_.batch_size);
            }
            step_start = std::chrono::steady_clock::now();
            cl_->read_buffer(results_buffer_, match_count * sizeof(GPUMatchResult),
                            gpu_results.data(), false);
            cl_->read_buffer(addresses_buffer_, match_count * TRX_ADDRESS_SIZE * sizeof(cl_uchar),
                            gpu_addresses.data(), true);
            step_end = std::chrono::steady_clock::now();
            batch_stats.result_read_ms = elapsed_ms(step_start, step_end);

            // Process GPU results on CPU (full verification)
            step_start = std::chrono::steady_clock::now();
            process_gpu_results(gpu_results, gpu_addresses, match_count, rng);
            step_end = std::chrono::steady_clock::now();
            batch_stats.host_process_ms = elapsed_ms(step_start, step_end);
        }
        batch_stats.batches = 1;
        batch_stats.matches_returned = match_count;
        {
            std::lock_guard<std::mutex> lock(profile_mutex_);
            profile_stats_.batches += batch_stats.batches;
            profile_stats_.matches_returned += batch_stats.matches_returned;
            profile_stats_.seed_generation_ms += batch_stats.seed_generation_ms;
            profile_stats_.seed_upload_ms += batch_stats.seed_upload_ms;
            profile_stats_.counter_reset_ms += batch_stats.counter_reset_ms;
            profile_stats_.kernel_ms += batch_stats.kernel_ms;
            profile_stats_.count_read_ms += batch_stats.count_read_ms;
            profile_stats_.result_read_ms += batch_stats.result_read_ms;
            profile_stats_.host_process_ms += batch_stats.host_process_ms;
        }

        total_attempts_ += config_.batch_size;

        if (config_.num_batches > 0 &&
            total_attempts_ >= config_.num_batches * config_.batch_size) {
            break;
        }
    }

    running_ = false;
}

void GPUGenerator::process_gpu_results(
    const std::vector<GPUMatchResult>& gpu_results,
    const std::vector<cl_uchar>& gpu_addresses,
    cl_uint count,
    RNG& rng
) {
    std::unique_ptr<Secp256k1> verifier;
    if (config_.verify_gpu_results) {
        verifier = std::make_unique<Secp256k1>();
    }

    for (cl_uint i = 0; i < count; ++i) {
        // Use the GPU-computed address for normal matching. Recomputing ECC on
        // the CPU for every candidate defeats the purpose of the full-GPU path;
        // keep it as an opt-in debug safeguard instead.
        std::array<uint8_t, TRX_ADDRESS_SIZE> address_bytes{};
        for (size_t j = 0; j < TRX_ADDRESS_SIZE; ++j) {
            address_bytes[j] = static_cast<uint8_t>(gpu_addresses[i * TRX_ADDRESS_SIZE + j]);
        }

        // Encode to Base58
        std::string address = Base58::encode_address(address_bytes);

        // Check the exact Base58 pattern before reconstructing the private key.
        // Matches are rare, so deferring CPU-side RNG/key formatting avoids per-
        // candidate work in the hot path.
        if (matcher_.matches_any(address)) {
            const auto& gpu_result = gpu_results[i];

            std::array<uint32_t, 4> seed = {
                gpu_result.seed[0],
                gpu_result.seed[1],
                gpu_result.seed[2],
                gpu_result.seed[3]
            };

            auto seed_bytes = rng.seed_to_bytes(seed);
            auto private_key = rng.derive_private_key(seed_bytes);

            if (config_.verify_gpu_results) {
                auto public_key = verifier->generate_public_key(private_key);
                auto cpu_address_bytes = verifier->generate_address_bytes(public_key);

                if (cpu_address_bytes != address_bytes) {
                    std::cerr << "WARNING: GPU/CPU address mismatch for seed "
                              << seed[0] << " " << seed[1] << " " << seed[2] << " " << seed[3] << "\n";
                    continue;
                }
            }

            std::string private_key_hex = bytes_to_hex(private_key.data(), PRIVATE_KEY_SIZE);
            auto patterns = matcher_.get_matching_patterns(address);
            MatchResult result;
            result.address = address;
            result.private_key_hex = private_key_hex;
            result.pattern_matched = patterns.empty() ? "" : patterns[0];
            result.attempts = total_attempts_.load() + gpu_result.reserved[0] + 1;

            {
                std::lock_guard<std::mutex> lock(results_mutex_);
                results_.push_back(result);
            }

            {
                std::lock_guard<std::mutex> lock(callback_mutex_);
                if (callback_) {
                    callback_(result);
                }
            }
        }
    }
}

std::vector<MatchResult> GPUGenerator::get_results() {
    std::lock_guard<std::mutex> lock(results_mutex_);
    return results_;
}

double GPUGenerator::get_rate() const {
    auto elapsed = std::chrono::steady_clock::now() - start_time_;
    double seconds = std::chrono::duration<double>(elapsed).count();
    if (seconds < 0.001) return 0.0;
    return static_cast<double>(total_attempts_.load()) / seconds;
}

GPUProfileStats GPUGenerator::get_profile_stats() const {
    std::lock_guard<std::mutex> lock(profile_mutex_);
    return profile_stats_;
}

} // namespace trx

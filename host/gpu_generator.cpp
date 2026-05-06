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

namespace trx {

namespace {

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
        // Generate seeds for this batch
        for (size_t i = 0; i < config_.batch_size; ++i) {
            auto seed = rng.generate_seed();
            seeds[i].s[0] = seed[0];
            seeds[i].s[1] = seed[1];
            seeds[i].s[2] = seed[2];
            seeds[i].s[3] = seed[3];
        }

        // Upload seeds
        cl_->write_buffer(seeds_buffer_, seeds.size() * sizeof(cl_uint4), seeds.data(), true);

        // Reset match count
        cl_uint zero = 0;
        cl_->write_buffer(match_count_buffer_, sizeof(cl_uint), &zero, true);

        // Launch kernel
        size_t global_size = config_.batch_size;
        size_t local_size = config_.work_group_size;

        // Round up to multiple of work group size
        if (global_size % local_size != 0) {
            global_size = ((global_size / local_size) + 1) * local_size;
        }

        cl_->enqueue_nd_range(kernel_, 1, &global_size, &local_size);
        cl_->finish();

        // Read results
        cl_uint match_count = 0;
        cl_->read_buffer(match_count_buffer_, sizeof(cl_uint), &match_count, true);

        if (match_count > 0) {
            if (match_count > config_.batch_size) {
                std::cerr << "WARNING: GPU returned match_count=" << match_count
                          << " greater than batch_size=" << config_.batch_size
                          << "; clamping readback to allocated buffer size\n";
                match_count = static_cast<cl_uint>(config_.batch_size);
            }
            cl_->read_buffer(results_buffer_, match_count * sizeof(GPUMatchResult),
                            gpu_results.data(), true);
            cl_->read_buffer(addresses_buffer_, match_count * TRX_ADDRESS_SIZE * sizeof(cl_uchar),
                            gpu_addresses.data(), true);

            // Process GPU results on CPU (full verification)
            process_gpu_results(gpu_results, gpu_addresses, match_count, rng);
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

} // namespace trx

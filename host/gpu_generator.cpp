#include "gpu_generator.h"
#include "utils/crypto.h"
#include "utils/rng.h"
#include <iostream>
#include <fstream>
#include <chrono>
#include <cstring>

namespace trx {

GPUGenerator::GPUGenerator()
    : kernel_(nullptr)
    , seeds_buffer_(nullptr)
    , results_buffer_(nullptr)
    , addresses_buffer_(nullptr)
    , match_count_buffer_(nullptr) {}

GPUGenerator::~GPUGenerator() {
    stop();
    if (cl_) {
        cl_->release_buffer(seeds_buffer_);
        cl_->release_buffer(results_buffer_);
        cl_->release_buffer(addresses_buffer_);
        cl_->release_buffer(match_count_buffer_);
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

    // Load Phase 3 full GPU kernel
    cl_->load_kernel("generate_addresses_full_gpu", "/Users/vincen/Vincen/code/trx_addr/kernel/vanity.cl");
    cl_->build_program("-cl-std=CL1.2 -Werror -DUSE_OPENCL=1");

    kernel_ = cl_->get_kernel("generate_addresses_full_gpu");

    // Create buffers
    size_t seeds_size = config_.batch_size * sizeof(cl_uint4);
    size_t results_size = MAX_RESULTS_PER_BATCH * sizeof(GPUMatchResult);
    size_t addresses_size = MAX_RESULTS_PER_BATCH * TRX_ADDRESS_SIZE * sizeof(cl_uchar);
    size_t match_count_size = sizeof(cl_uint);

    seeds_buffer_ = cl_->create_buffer(CL_MEM_READ_ONLY, seeds_size);
    results_buffer_ = cl_->create_buffer(CL_MEM_WRITE_ONLY, results_size);
    addresses_buffer_ = cl_->create_buffer(CL_MEM_WRITE_ONLY, addresses_size);
    match_count_buffer_ = cl_->create_buffer(CL_MEM_READ_WRITE, match_count_size);

    // Set kernel args (0=seeds, 1=results, 2=match_count, 3=addresses_out, 4=batch_size)
    cl_->set_kernel_arg_buffer(kernel_, 0, seeds_buffer_);
    cl_->set_kernel_arg_buffer(kernel_, 1, results_buffer_);
    cl_->set_kernel_arg_buffer(kernel_, 2, match_count_buffer_);
    cl_->set_kernel_arg_buffer(kernel_, 3, addresses_buffer_);
    cl_->set_kernel_arg(kernel_, 4, sizeof(cl_uint), &config_.batch_size);
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
    Secp256k1 ecc;
    std::vector<cl_uint4> seeds(config_.batch_size);
    std::vector<GPUMatchResult> gpu_results(MAX_RESULTS_PER_BATCH);
    std::vector<cl_uchar> gpu_addresses(MAX_RESULTS_PER_BATCH * TRX_ADDRESS_SIZE);

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
            match_count = std::min(match_count, static_cast<cl_uint>(MAX_RESULTS_PER_BATCH));
            cl_->read_buffer(results_buffer_, match_count * sizeof(GPUMatchResult),
                            gpu_results.data(), true);
            cl_->read_buffer(addresses_buffer_, match_count * TRX_ADDRESS_SIZE * sizeof(cl_uchar),
                            gpu_addresses.data(), true);

            // Process GPU results on CPU (full verification)
            process_gpu_results(gpu_results, gpu_addresses, match_count, seeds, ecc, rng);
        }

        total_attempts_ += config_.batch_size;

        if (config_.num_batches > 0 &&
            total_attempts_ >= config_.num_batches * config_.batch_size) {
            break;
        }
    }
}

void GPUGenerator::process_gpu_results(
    const std::vector<GPUMatchResult>& gpu_results,
    const std::vector<cl_uchar>& gpu_addresses,
    cl_uint count,
    const std::vector<cl_uint4>& seeds,
    Secp256k1& ecc,
    RNG& rng
) {
    for (cl_uint i = 0; i < count; ++i) {
        const auto& gpu_result = gpu_results[i];

        // Reconstruct seed
        std::array<uint32_t, 4> seed = {
            gpu_result.seed[0],
            gpu_result.seed[1],
            gpu_result.seed[2],
            gpu_result.seed[3]
        };

        // Regenerate private key from seed
        auto seed_bytes = rng.seed_to_bytes(seed);
        auto private_key = rng.derive_private_key(seed_bytes);

        // Generate public key and address using CPU reference
        auto public_key = ecc.generate_public_key(private_key);
        auto address_bytes = ecc.generate_address_bytes(public_key);

        // Compare with GPU output
        bool gpu_match = true;
        for (size_t j = 0; j < TRX_ADDRESS_SIZE; ++j) {
            if (gpu_addresses[i * TRX_ADDRESS_SIZE + j] != address_bytes[j]) {
                gpu_match = false;
                break;
            }
        }

        if (!gpu_match) {
            std::cerr << "WARNING: GPU/CPU address mismatch for seed "
                      << seed[0] << " " << seed[1] << " " << seed[2] << " " << seed[3] << "\n";
            continue;
        }

        // Encode to Base58
        std::string address = Base58::encode_address(address_bytes);
        std::string private_key_hex = bytes_to_hex(private_key.data(), PRIVATE_KEY_SIZE);

        // Check if it actually matches our patterns
        if (matcher_.matches_any(address)) {
            auto patterns = matcher_.get_matching_patterns(address);
            MatchResult result;
            result.address = address;
            result.private_key_hex = private_key_hex;
            result.pattern_matched = patterns.empty() ? "" : patterns[0];
            result.attempts = total_attempts_.load();

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

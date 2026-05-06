#include "cpu_generator.h"
#include <iostream>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <fstream>

namespace trx {

CPUGenerator::CPUGenerator()
    : ecc_(std::make_unique<Secp256k1>())
    , rng_(std::make_unique<RNG>())
    , num_threads_(std::thread::hardware_concurrency())
    , batch_size_(10000) {}

CPUGenerator::~CPUGenerator() {
    stop();
}

void CPUGenerator::set_pattern(std::unique_ptr<Pattern> pattern) {
    matcher_ = MultiPatternMatcher();
    matcher_.add_pattern(std::move(pattern));
}

void CPUGenerator::add_pattern(std::unique_ptr<Pattern> pattern) {
    matcher_.add_pattern(std::move(pattern));
}

void CPUGenerator::set_num_threads(size_t threads) {
    num_threads_ = threads;
}

void CPUGenerator::set_batch_size(size_t size) {
    batch_size_ = size;
}

void CPUGenerator::set_max_attempts(uint64_t max_attempts) {
    max_attempts_ = max_attempts;
}

void CPUGenerator::set_callback(ResultCallback cb) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    callback_ = cb;
}

void CPUGenerator::start() {
    if (running_.load()) return;

    running_ = true;
    stop_requested_ = false;
    total_attempts_ = 0;
    match_count_ = 0;
    start_time_ = std::chrono::steady_clock::now();
    results_.clear();

    // Launch worker threads
    for (size_t i = 0; i < num_threads_; ++i) {
        threads_.emplace_back(&CPUGenerator::worker_thread, this, static_cast<int>(i));
    }
}

void CPUGenerator::stop() {
    stop_requested_ = true;
    running_ = false;

    for (auto& t : threads_) {
        if (t.joinable()) {
            t.join();
        }
    }
    threads_.clear();
}

void CPUGenerator::worker_thread(int thread_id) {
    // Each thread has its own RNG seeded differently
    RNG local_rng(static_cast<uint64_t>(thread_id) +
                  std::chrono::high_resolution_clock::now().time_since_epoch().count());

    Secp256k1 local_ecc;

    while (!stop_requested_.load()) {
        // Generate batch of private keys
        for (size_t i = 0; i < batch_size_ && !stop_requested_.load(); ++i) {
            auto private_key = local_rng.generate_private_key();

            // ECC: private key -> public key
            auto public_key = local_ecc.generate_public_key(private_key);

            // Keccak256 -> address bytes
            auto address_bytes = local_ecc.generate_address_bytes(public_key);

            // Base58 encode
            std::string address = Base58::encode_address(address_bytes);

            // Check pattern match
            if (matcher_.matches_any(address)) {
                MatchResult result;
                result.address = address;
                result.private_key_hex = bytes_to_hex(private_key.data(), PRIVATE_KEY_SIZE);
                result.pattern_matched = "matched"; // Simplified
                result.attempts = total_attempts_.load();

                {
                    std::lock_guard<std::mutex> lock(results_mutex_);
                    results_.push_back(result);
                }

                match_count_++;

                // Callback
                {
                    std::lock_guard<std::mutex> lock(callback_mutex_);
                    if (callback_) {
                        callback_(result);
                    }
                }
            }

            uint64_t attempts = ++total_attempts_;
            if (max_attempts_ > 0 && attempts >= max_attempts_) {
                stop_requested_ = true;
                running_ = false;
                break;
            }
        }
    }
}

std::vector<MatchResult> CPUGenerator::get_results() {
    std::lock_guard<std::mutex> lock(results_mutex_);
    return results_;
}

double CPUGenerator::get_rate() const {
    auto elapsed = std::chrono::steady_clock::now() - start_time_;
    double seconds = std::chrono::duration<double>(elapsed).count();
    if (seconds < 0.001) return 0.0;
    return static_cast<double>(total_attempts_.load()) / seconds;
}

} // namespace trx

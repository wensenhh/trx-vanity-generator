#ifndef TRX_CPU_GENERATOR_H
#define TRX_CPU_GENERATOR_H

#include "utils/constants.h"
#include "utils/crypto.h"
#include "utils/pattern.h"
#include "utils/rng.h"
#include <atomic>
#include <thread>
#include <vector>
#include <memory>
#include <functional>

namespace trx {

// ============================================================================
// CPU Vanity Address Generator
// Phase 1: Complete working implementation
// ============================================================================

class CPUGenerator {
public:
    CPUGenerator();
    ~CPUGenerator();

    // Configuration
    void set_pattern(std::unique_ptr<Pattern> pattern);
    void add_pattern(std::unique_ptr<Pattern> pattern);
    void set_num_threads(size_t threads);
    void set_batch_size(size_t size);
    void set_max_attempts(uint64_t max_attempts);

    // Generation control
    void start();
    void stop();
    bool is_running() const;

    // Results
    std::vector<MatchResult> get_results();
    uint64_t get_total_attempts() const { return total_attempts_.load(); }
    double get_rate() const; // addresses per second

    // Callback for real-time results
    using ResultCallback = std::function<void(const MatchResult&)>;
    void set_callback(ResultCallback cb);

private:
    void worker_thread(int thread_id);
    void process_batch();

    // Components
    std::unique_ptr<Secp256k1> ecc_;
    std::unique_ptr<RNG> rng_;
    MultiPatternMatcher matcher_;

    // Threading
    std::vector<std::thread> threads_;
    size_t num_threads_;
    size_t batch_size_;
    std::atomic<bool> running_{false};
    std::atomic<bool> stop_requested_{false};

    // Statistics
    std::atomic<uint64_t> total_attempts_{0};
    std::atomic<uint64_t> match_count_{0};
    uint64_t max_attempts_{0}; // 0 = unlimited
    std::chrono::steady_clock::time_point start_time_;

    // Results
    std::vector<MatchResult> results_;
    std::mutex results_mutex_;

    // Callback
    ResultCallback callback_;
    std::mutex callback_mutex_;
};

} // namespace trx

#endif // TRX_CPU_GENERATOR_H
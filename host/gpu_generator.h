#ifndef TRX_GPU_GENERATOR_H
#define TRX_GPU_GENERATOR_H

#include "utils/constants.h"
#include "utils/pattern.h"
#include "utils/crypto.h"
#include "utils/rng.h"
#include "opencl_manager.h"
#include <atomic>
#include <thread>
#include <vector>
#include <memory>

namespace trx {

// ============================================================================
// GPU Vanity Address Generator (Phase 2-3)
// Uses OpenCL for GPU acceleration
// ============================================================================

struct GPUGenerationConfig {
    size_t batch_size = 65536;        // Addresses per batch
    size_t work_group_size = 256;     // OpenCL work group size
    size_t num_batches = 0;           // 0 = infinite
    int platform_idx = -1;            // -1 = auto
    int device_idx = -1;              // -1 = auto
    bool verbose = false;
};

class GPUGenerator {
public:
    GPUGenerator();
    ~GPUGenerator();

    // Configuration
    void set_config(const GPUGenerationConfig& config);
    void set_pattern(std::unique_ptr<Pattern> pattern);
    void add_pattern(std::unique_ptr<Pattern> pattern);

    // Pattern data for GPU
    void upload_pattern_data();

    // Generation control
    void initialize();
    void start();
    void stop();
    bool is_running() const { return running_.load(); }

    // Results
    std::vector<MatchResult> get_results();
    uint64_t get_total_attempts() const { return total_attempts_.load(); }
    double get_rate() const;

    // Callback
    using ResultCallback = std::function<void(const MatchResult&)>;
    void set_callback(ResultCallback cb);

private:
    void generation_loop();
    void process_gpu_results(
        const std::vector<GPUMatchResult>& gpu_results,
        const std::vector<cl_uchar>& gpu_addresses,
        cl_uint count,
        const std::vector<cl_uint4>& seeds,
        Secp256k1& ecc,
        RNG& rng
    );

    // OpenCL
    std::unique_ptr<OpenCLManager> cl_;
    cl_kernel kernel_;

    // Buffers
    cl_mem seeds_buffer_;
    cl_mem results_buffer_;
    cl_mem addresses_buffer_;
    cl_mem match_count_buffer_;

    // Pattern
    MultiPatternMatcher matcher_;
    std::vector<uint8_t> pattern_gpu_data_;

    // Config
    GPUGenerationConfig config_;

    // Threading
    std::thread generation_thread_;
    std::atomic<bool> running_{false};
    std::atomic<bool> stop_requested_{false};

    // Statistics
    std::atomic<uint64_t> total_attempts_{0};
    std::chrono::steady_clock::time_point start_time_;

    // Results
    std::vector<MatchResult> results_;
    std::mutex results_mutex_;

    // Callback
    ResultCallback callback_;
    std::mutex callback_mutex_;
};

} // namespace trx

#endif // TRX_GPU_GENERATOR_H

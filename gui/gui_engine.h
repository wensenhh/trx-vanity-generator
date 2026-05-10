#ifndef TRX_GUI_ENGINE_H
#define TRX_GUI_ENGINE_H

#include "utils/constants.h"
#include "utils/crypto.h"
#include "utils/pattern.h"
#include "utils/rng.h"
#include "host/cpu_generator.h"
#include "host/gpu_generator.h"
#include <atomic>
#include <mutex>
#include <vector>
#include <string>
#include <functional>
#include <chrono>
#include <memory>

namespace trx {

// ============================================================================
// GUI Engine — thin wrapper around CPUGenerator with pause/resume
// and private-key-safe callbacks.
// ============================================================================

struct GUIMatchResult {
    std::string address;
    std::string private_key_hex;   // hex (64 chars), default hidden
    std::string pattern_matched;
    uint64_t attempts;
    std::chrono::steady_clock::time_point timestamp;
};

struct GUIStats {
    uint64_t total_attempts{0};
    uint64_t match_count{0};
    double rate_per_second{0.0};   // addresses/sec
    double elapsed_seconds{0.0};
    bool is_running{false};
    bool is_paused{false};
};

enum class GUIMode {
    CPU,
    GPU
};

class GUIEngine {
public:
    GUIEngine();
    ~GUIEngine();

    // Configuration
    void set_pattern(std::unique_ptr<Pattern> pattern);
    void set_mode(GUIMode mode);
    void set_num_threads(size_t threads);
    void set_batch_size(size_t size);   // GPU batch size
    void set_max_attempts(uint64_t max); // 0 = unlimited

    // Control
    void start();
    void pause();   // keeps threads alive but stops generating
    void resume();  // resume from pause
    void stop();    // terminate threads

    // Queries
    GUIStats get_stats() const;
    std::vector<GUIMatchResult> get_matches();
    bool is_running() const;
    bool is_paused() const;

    // Export
    enum class ExportFormat { CSV, JSON };
    bool export_matches(const std::string& filepath,
                        ExportFormat format,
                        const std::string& passphrase,
                        std::string& error_out);

    // Callbacks (thread-safe, called from worker threads)
    using MatchCallback = std::function<void(const GUIMatchResult&)>;
    using StatsCallback = std::function<void(const GUIStats&)>;
    void set_match_callback(MatchCallback cb);
    void set_stats_callback(StatsCallback cb);

private:
    void on_match(const MatchResult& raw);
    void on_tick(); // periodic stats update

    // Underlying generator
    std::unique_ptr<CPUGenerator> cpu_gen_;
#ifdef USE_OPENCL
    std::unique_ptr<GPUGenerator> gpu_gen_;
#endif

    GUIMode mode_{GUIMode::CPU};
    std::atomic<bool> running_{false};
    std::atomic<bool> paused_{false};
    std::atomic<bool> stop_requested_{false};

    // Stats
    mutable std::mutex stats_mutex_;
    GUIStats stats_;
    std::chrono::steady_clock::time_point start_time_;

    // Matches
    mutable std::mutex matches_mutex_;
    std::vector<GUIMatchResult> matches_;

    // Callbacks
    MatchCallback match_cb_;
    StatsCallback stats_cb_;
    mutable std::mutex cb_mutex_;

    // Background stats ticker thread
    std::thread ticker_thread_;
    void ticker_loop();

    // Cached configuration
    uint64_t max_attempts_{0}; // 0 = unlimited
    size_t num_threads_{0};    // 0 = use default
    size_t batch_size_{0};     // 0 = use default

    // Pattern params for GPU recreation
    PatternType pattern_type_{PatternType::SUFFIX_CUSTOM};
    std::string pattern_param1_;
    std::string pattern_param2_;
};

} // namespace trx

#endif // TRX_GUI_ENGINE_H

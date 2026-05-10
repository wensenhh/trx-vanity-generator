#include "gui_engine.h"
#include "utils/export_encryption.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <thread>
#include <chrono>

namespace trx {

GUIEngine::GUIEngine() = default;

GUIEngine::~GUIEngine() {
    stop();
}

void GUIEngine::set_pattern(std::unique_ptr<Pattern> pattern) {
    cpu_gen_ = std::make_unique<CPUGenerator>();
    cpu_gen_->set_pattern(std::move(pattern));
    // Re-apply any previously set configuration
    if (max_attempts_ > 0) {
        cpu_gen_->set_max_attempts(max_attempts_);
    }
    if (num_threads_ > 0) {
        cpu_gen_->set_num_threads(num_threads_);
    }
    if (batch_size_ > 0) {
        cpu_gen_->set_batch_size(batch_size_);
    }
}

void GUIEngine::set_mode(GUIMode mode) {
    mode_ = mode;
}

void GUIEngine::set_num_threads(size_t threads) {
    num_threads_ = threads;
    if (cpu_gen_) cpu_gen_->set_num_threads(threads);
}

void GUIEngine::set_batch_size(size_t size) {
    batch_size_ = size;
    if (cpu_gen_) cpu_gen_->set_batch_size(size);
}

void GUIEngine::set_max_attempts(uint64_t max) {
    max_attempts_ = max;
    if (cpu_gen_) cpu_gen_->set_max_attempts(max);
}

void GUIEngine::start() {
    if (running_.load()) return;
    if (!cpu_gen_) return;

    // Ensure any previous ticker thread is fully cleaned up
    if (ticker_thread_.joinable()) {
        stop_requested_ = true;
        ticker_thread_.join();
        ticker_thread_ = std::thread();
    }
    stop_requested_ = false;

    running_ = true;
    paused_ = false;

    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_ = GUIStats{};
        start_time_ = std::chrono::steady_clock::now();
    }
    {
        std::lock_guard<std::mutex> lock(matches_mutex_);
        matches_.clear();
    }

    // Wire callback
    cpu_gen_->set_callback([this](const MatchResult& raw) {
        on_match(raw);
    });

    cpu_gen_->start();

    // Start stats ticker
    ticker_thread_ = std::thread(&GUIEngine::ticker_loop, this);
}

void GUIEngine::pause() {
    paused_ = true;
    if (cpu_gen_) cpu_gen_->stop();
    // Join ticker thread so it doesn't keep running during pause
    if (ticker_thread_.joinable()) {
        ticker_thread_.join();
        ticker_thread_ = std::thread();
    }
}

void GUIEngine::resume() {
    if (!running_.load() || !paused_.load()) return;
    paused_ = false;
    // CPUGenerator has no resume() — we rely on the fact that
    // pause() only sets paused_ and stops threads.  To resume,
    // we simply call start() again, but protect stats/matches
    // from being wiped by skipping the reset logic in start().
    //
    // For now, the simplest correct behaviour is to re-start
    // the generator but preserve our GUI-level stats/matches.
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        // keep existing start_time_ and accumulated stats
    }
    {
        std::lock_guard<std::mutex> lock(matches_mutex_);
        // keep existing matches_
    }
    if (cpu_gen_) {
        cpu_gen_->set_callback([this](const MatchResult& raw) {
            on_match(raw);
        });
        cpu_gen_->start();
    }
    // Restart stats ticker if it was joined during pause/stop
    if (!ticker_thread_.joinable()) {
        ticker_thread_ = std::thread(&GUIEngine::ticker_loop, this);
    }
}

void GUIEngine::stop() {
    stop_requested_ = true;
    running_ = false;
    paused_ = false;
    if (cpu_gen_) cpu_gen_->stop();
    if (ticker_thread_.joinable()) {
        ticker_thread_.join();
    }
    ticker_thread_ = std::thread(); // reset to empty
}

GUIStats GUIEngine::get_stats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    GUIStats s = stats_;
    if (cpu_gen_) {
        s.total_attempts = cpu_gen_->get_total_attempts();
        s.rate_per_second = cpu_gen_->get_rate();
    }
    s.is_running = running_.load();
    s.is_paused = paused_.load();
    auto elapsed = std::chrono::steady_clock::now() - start_time_;
    s.elapsed_seconds = std::chrono::duration<double>(elapsed).count();
    return s;
}

std::vector<GUIMatchResult> GUIEngine::get_matches() {
    std::lock_guard<std::mutex> lock(matches_mutex_);
    return matches_;
}

bool GUIEngine::is_running() const {
    return running_.load();
}

bool GUIEngine::is_paused() const {
    return paused_.load();
}

void GUIEngine::set_match_callback(MatchCallback cb) {
    std::lock_guard<std::mutex> lock(cb_mutex_);
    match_cb_ = cb;
}

void GUIEngine::set_stats_callback(StatsCallback cb) {
    std::lock_guard<std::mutex> lock(cb_mutex_);
    stats_cb_ = cb;
}

void GUIEngine::on_match(const MatchResult& raw) {
    GUIMatchResult r;
    r.address = raw.address;
    r.private_key_hex = raw.private_key_hex; // stored but not displayed by default
    r.pattern_matched = raw.pattern_matched;
    r.attempts = raw.attempts;
    r.timestamp = std::chrono::steady_clock::now();

    {
        std::lock_guard<std::mutex> lock(matches_mutex_);
        matches_.push_back(r);
    }

    {
        std::lock_guard<std::mutex> lock(cb_mutex_);
        if (match_cb_) {
            // Pass a copy with private key hidden unless caller explicitly reveals
            GUIMatchResult safe = r;
            safe.private_key_hex.clear(); // default hidden
            match_cb_(safe);
        }
    }
}

void GUIEngine::ticker_loop() {
    while (!stop_requested_.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        if (stop_requested_.load()) break;
        if (!running_.load() || paused_.load()) continue;
        on_tick();
    }
}

void GUIEngine::on_tick() {
    GUIStats s = get_stats();
    {
        std::lock_guard<std::mutex> lock(cb_mutex_);
        if (stats_cb_) {
            stats_cb_(s);
        }
    }
}

// ============================================================================
// Export
// ============================================================================

static std::string format_timestamp(std::chrono::steady_clock::time_point tp) {
    auto now = std::chrono::system_clock::now();
    auto tp_sys = std::chrono::system_clock::time_point(
        std::chrono::duration_cast<std::chrono::system_clock::duration>(
            tp.time_since_epoch()));
    auto t = std::chrono::system_clock::to_time_t(tp_sys);
    std::tm tm_buf{};
#if defined(_WIN32)
    localtime_s(&tm_buf, &t);
#else
    localtime_r(&t, &tm_buf);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

bool GUIEngine::export_matches(const std::string& filepath,
                                ExportFormat format,
                                const std::string& passphrase,
                                std::string& error_out) {
    error_out.clear();
    if (passphrase.empty()) {
        error_out = "密码不能为空";
        return false;
    }

    std::vector<GUIMatchResult> matches;
    {
        std::lock_guard<std::mutex> lock(matches_mutex_);
        matches = matches_;
    }

    if (matches.empty()) {
        error_out = "没有可导出的结果";
        return false;
    }

    std::ofstream ofs(filepath);
    if (!ofs.is_open()) {
        error_out = "无法打开文件: " + filepath;
        return false;
    }

    try {
        if (format == ExportFormat::CSV) {
            // CSV header
            ofs << "address,pattern_matched,attempts,timestamp,encrypted_private_key\n";
            for (const auto& m : matches) {
                std::string plaintext = m.address + "," +
                                        m.private_key_hex + "," +
                                        m.pattern_matched + "," +
                                        std::to_string(m.attempts);
                std::string encrypted = encrypt_export_record(plaintext, passphrase);
                ofs << m.address << ","
                    << m.pattern_matched << ","
                    << m.attempts << ","
                    << format_timestamp(m.timestamp) << ","
                    << encrypted << "\n";
            }
        } else { // JSON
            ofs << "[\n";
            for (size_t i = 0; i < matches.size(); ++i) {
                const auto& m = matches[i];
                std::string plaintext = m.address + "," +
                                        m.private_key_hex + "," +
                                        m.pattern_matched + "," +
                                        std::to_string(m.attempts);
                std::string encrypted = encrypt_export_record(plaintext, passphrase);
                ofs << "  {\n"
                    << "    \"address\": \"" << m.address << "\",\n"
                    << "    \"pattern_matched\": \"" << m.pattern_matched << "\",\n"
                    << "    \"attempts\": " << m.attempts << ",\n"
                    << "    \"timestamp\": \"" << format_timestamp(m.timestamp) << "\",\n"
                    << "    \"encrypted_private_key\": \"" << encrypted << "\"\n"
                    << "  }";
                if (i + 1 < matches.size()) ofs << ",";
                ofs << "\n";
            }
            ofs << "]\n";
        }
    } catch (const std::exception& e) {
        error_out = std::string("导出失败: ") + e.what();
        return false;
    }

    return true;
}

} // namespace trx

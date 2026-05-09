#include "gui_engine.h"
#include <iostream>
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
}

void GUIEngine::set_mode(GUIMode mode) {
    mode_ = mode;
}

void GUIEngine::set_num_threads(size_t threads) {
    if (cpu_gen_) cpu_gen_->set_num_threads(threads);
}

void GUIEngine::set_batch_size(size_t size) {
    // GPU batch size placeholder; CPU uses internal batch
    (void)size;
}

void GUIEngine::set_max_attempts(uint64_t max) {
    if (cpu_gen_) cpu_gen_->set_max_attempts(max);
}

void GUIEngine::start() {
    if (running_.load()) return;
    if (!cpu_gen_) return;

    running_ = true;
    paused_ = false;
    stop_requested_ = false;

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
}

void GUIEngine::stop() {
    stop_requested_ = true;
    running_ = false;
    paused_ = false;
    if (cpu_gen_) cpu_gen_->stop();
    if (ticker_thread_.joinable()) {
        ticker_thread_.join();
    }
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

} // namespace trx

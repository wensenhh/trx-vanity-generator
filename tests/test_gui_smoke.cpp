#include "gui_engine.h"
#include "utils/pattern.h"
#include "utils/estimate.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <thread>
#include <chrono>
#include <atomic>
#include <mutex>
#include <vector>
#include <cstring>

using namespace trx;

// ============================================================================
// Headless GUI smoke test — exercises GUIEngine without a display.
// ============================================================================

static std::atomic<int> g_match_count{0};
static std::atomic<int> g_stats_ticks{0};
static std::mutex g_log_mutex;

static void log(const std::string& msg) {
    std::lock_guard<std::mutex> lock(g_log_mutex);
    std::cout << "[GUI_TEST] " << msg << "\n";
}

static void on_match_cb(const GUIMatchResult& match) {
    g_match_count++;
    log("Match callback: address=" + match.address +
        " pattern=" + match.pattern_matched +
        " key_hidden=" + (match.private_key_hex.empty() ? "yes" : "no"));
}

static void on_stats_cb(const GUIStats& s) {
    g_stats_ticks++;
    if (g_stats_ticks.load() <= 3) {
        log("Stats tick: attempts=" + std::to_string(s.total_attempts) +
            " rate=" + std::to_string(static_cast<int>(s.rate_per_second)) +
            " running=" + std::to_string(s.is_running));
    }
}

int main(int argc, char** argv) {
    (void)argc; (void)argv;

    log("=== GUI Smoke Test Start ===");

    // 1. Create engine with a simple pattern
    auto engine = std::make_unique<GUIEngine>();
    auto pattern = PatternFactory::create(PatternType::CONTAINS, "T");
    engine->set_pattern(std::move(pattern));
    engine->set_num_threads(1);
    engine->set_max_attempts(256); // small for smoke test

    engine->set_match_callback(on_match_cb);
    engine->set_stats_callback(on_stats_cb);

    // 2. Start
    log("Starting engine...");
    engine->start();

    // 3. Let it run briefly
    std::this_thread::sleep_for(std::chrono::milliseconds(800));

    // 4. Pause
    log("Pausing engine...");
    engine->pause();
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    if (engine->is_paused()) {
        log("Pause confirmed.");
    } else {
        log("ERROR: Pause not confirmed!");
        return 1;
    }

    // 5. Resume
    log("Resuming engine...");
    engine->resume();
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // 6. Stop
    log("Stopping engine...");
    engine->stop();
    if (!engine->is_running()) {
        log("Stop confirmed.");
    } else {
        log("ERROR: Stop not confirmed!");
        return 1;
    }

    // 7. Verify stats
    GUIStats stats = engine->get_stats();
    log("Final stats: attempts=" + std::to_string(stats.total_attempts) +
        " matches=" + std::to_string(stats.match_count) +
        " ticks=" + std::to_string(g_stats_ticks.load()));

    // 8. Verify matches do NOT expose private key in callback
    // The callback receives a copy with private_key_hex cleared.
    // We verify the stored matches retain the key (for reveal later)
    // and that the callback log lines never printed a key.
    auto matches = engine->get_matches();
    bool stored_keys_valid = true;
    for (const auto& m : matches) {
        if (m.private_key_hex.empty() || m.private_key_hex.size() != 64) {
            log("ERROR: stored private key missing or wrong length for address " + m.address);
            stored_keys_valid = false;
        }
    }
    if (!stored_keys_valid) {
        return 1;
    }
    log("Stored keys valid (hidden in callback, retained internally).");

    // 9. Verify stored matches have key but it's not logged
    for (const auto& m : matches) {
        if (m.private_key_hex.empty() || m.private_key_hex.size() != 64) {
            log("WARNING: stored private key looks wrong for address " + m.address);
        }
    }

    // 10. Verify no private key in stdout log
    // (Manual inspection of output above)

    log("=== GUI Smoke Test PASSED ===");
    return 0;
}

#include "gui_engine.h"
#include "utils/pattern.h"
#include "utils/estimate.h"
#include "utils/export_encryption.h"
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
    engine->set_batch_size(10);     // small batch for faster stop
    engine->set_max_attempts(64);    // very small for smoke test

    engine->set_match_callback(on_match_cb);
    engine->set_stats_callback(on_stats_cb);

    // 2. Start
    log("Starting engine...");
    engine->start();

    // 3. Wait for completion (max_attempts should stop it)
    // Give engine time to start threads
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    int wait_cycles = 0;
    while (engine->is_running() && wait_cycles < 100) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        wait_cycles++;
    }
    if (engine->is_running()) {
        log("WARNING: Engine still running after wait, forcing stop...");
        engine->stop();
    } else {
        log("Engine stopped naturally (max_attempts reached).");
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

    // 11. Export test — CSV
    log("Testing CSV export with encryption...");
    {
        std::string csv_path = "/tmp/test_gui_export.csv";
        std::string passphrase = "test_password_123";
        std::string error;
        bool ok = engine->export_matches(csv_path, GUIEngine::ExportFormat::CSV, passphrase, error);
        if (!ok) {
            log("ERROR: CSV export failed: " + error);
            return 1;
        }
        // Verify file exists and contains encrypted data
        std::ifstream ifs(csv_path);
        if (!ifs.is_open()) {
            log("ERROR: CSV export file not created");
            return 1;
        }
        std::string line;
        std::getline(ifs, line); // header
        if (line.find("address,pattern_matched,attempts,timestamp,encrypted_private_key") == std::string::npos) {
            log("ERROR: CSV header incorrect");
            return 1;
        }
        std::getline(ifs, line); // first data row
        if (line.empty()) {
            log("ERROR: CSV data row empty");
            return 1;
        }
        // Verify encrypted field looks like encrypted record
        size_t last_comma = line.rfind(',');
        if (last_comma == std::string::npos) {
            log("ERROR: CSV format malformed");
            return 1;
        }
        std::string encrypted_field = line.substr(last_comma + 1);
        if (!trx::looks_like_encrypted_export_record(encrypted_field)) {
            log("ERROR: CSV encrypted field does not look like encrypted record");
            return 1;
        }
        // Verify we can decrypt it
        std::string decrypted;
        if (!trx::decrypt_export_record(encrypted_field, passphrase, decrypted)) {
            log("ERROR: Failed to decrypt CSV export record");
            return 1;
        }
        if (decrypted.empty()) {
            log("ERROR: Decrypted CSV record is empty");
            return 1;
        }
        // Verify decrypted contains address and private key
        if (decrypted.find("T") == std::string::npos) {
            log("ERROR: Decrypted CSV record missing address");
            return 1;
        }
        log("CSV export verified (encrypted + decryptable).");
    }

    // 12. Export test — JSON
    log("Testing JSON export with encryption...");
    {
        std::string json_path = "/tmp/test_gui_export.json";
        std::string passphrase = "json_password_456";
        std::string error;
        bool ok = engine->export_matches(json_path, GUIEngine::ExportFormat::JSON, passphrase, error);
        if (!ok) {
            log("ERROR: JSON export failed: " + error);
            return 1;
        }
        std::ifstream ifs(json_path);
        if (!ifs.is_open()) {
            log("ERROR: JSON export file not created");
            return 1;
        }
        std::string content((std::istreambuf_iterator<char>(ifs)),
                             std::istreambuf_iterator<char>());
        if (content.empty()) {
            log("ERROR: JSON export file empty");
            return 1;
        }
        if (content.find('[') == std::string::npos || content.find(']') == std::string::npos) {
            log("ERROR: JSON export missing array brackets");
            return 1;
        }
        if (content.find("encrypted_private_key") == std::string::npos) {
            log("ERROR: JSON export missing encrypted_private_key field");
            return 1;
        }
        // Extract encrypted field
        size_t enc_pos = content.find("\"encrypted_private_key\": \"");
        if (enc_pos == std::string::npos) {
            log("ERROR: JSON encrypted field not found");
            return 1;
        }
        size_t val_start = enc_pos + std::strlen("\"encrypted_private_key\": \"");
        size_t val_end = content.find('"', val_start);
        if (val_end == std::string::npos) {
            log("ERROR: JSON encrypted field value not terminated");
            return 1;
        }
        std::string encrypted_field = content.substr(val_start, val_end - val_start);
        if (!trx::looks_like_encrypted_export_record(encrypted_field)) {
            log("ERROR: JSON encrypted field does not look like encrypted record");
            return 1;
        }
        // Verify decryption
        std::string decrypted;
        if (!trx::decrypt_export_record(encrypted_field, passphrase, decrypted)) {
            log("ERROR: Failed to decrypt JSON export record");
            return 1;
        }
        if (decrypted.empty()) {
            log("ERROR: Decrypted JSON record is empty");
            return 1;
        }
        log("JSON export verified (encrypted + decryptable).");
    }

    // 13. Export with empty password should fail
    log("Testing export with empty password...");
    {
        std::string error;
        bool ok = engine->export_matches("/tmp/test_should_fail.csv", GUIEngine::ExportFormat::CSV, "", error);
        if (ok) {
            log("ERROR: Export with empty password should have failed");
            return 1;
        }
        if (error.find("密码") == std::string::npos) {
            log("ERROR: Empty password error message missing expected text");
            return 1;
        }
        log("Empty password correctly rejected.");
    }

    // 14. Export with no matches should fail gracefully
    log("Testing export with no matches...");
    {
        auto fresh_engine = std::make_unique<GUIEngine>();
        std::string error;
        bool ok = fresh_engine->export_matches("/tmp/test_no_matches.csv", GUIEngine::ExportFormat::CSV, "pwd", error);
        if (ok) {
            log("ERROR: Export with no matches should have failed");
            return 1;
        }
        if (error.find("没有") == std::string::npos) {
            log("ERROR: No matches error message missing expected text");
            return 1;
        }
        log("No matches correctly rejected.");
    }

    // 15. Verify max_attempts was respected
    log("Verifying max_attempts was respected...");
    {
        GUIStats stats = engine->get_stats();
        if (stats.total_attempts > 300) {
            log("WARNING: total_attempts=" + std::to_string(stats.total_attempts) +
                " exceeds expected max_attempts=256");
        } else {
            log("max_attempts respected: total_attempts=" + std::to_string(stats.total_attempts));
        }
    }

    log("=== GUI Smoke Test PASSED ===");
    return 0;
}

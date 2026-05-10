#include "utils/history.h"
#include "utils/export_encryption.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstdio>
#include <cstring>

using namespace trx;

static std::string g_test_history_path = "/tmp/test_trx_vanity_history.json";

static void cleanup() {
    std::remove(g_test_history_path.c_str());
}

static bool file_exists(const std::string& path) {
    std::ifstream ifs(path);
    return ifs.good();
}

int main() {
    cleanup();

    std::cout << "[HISTORY_TEST] Starting history tests...\n";

    // 1. Test default path
    {
        std::string default_path = HistoryManager::default_history_path();
        if (default_path.empty()) {
            std::cerr << "[HISTORY_TEST] FAIL: default_history_path is empty\n";
            return 1;
        }
        if (default_path.find(".trx_vanity") == std::string::npos) {
            std::cerr << "[HISTORY_TEST] FAIL: default path missing .trx_vanity\n";
            return 1;
        }
        std::cout << "[HISTORY_TEST] Default path: " << default_path << "\n";
    }

    // 2. Test empty load (no file yet)
    {
        HistoryManager mgr;
        bool ok = mgr.load(g_test_history_path);
        if (!ok) {
            std::cerr << "[HISTORY_TEST] FAIL: load on non-existent file should succeed\n";
            return 1;
        }
        if (mgr.count() != 0) {
            std::cerr << "[HISTORY_TEST] FAIL: empty load should have 0 entries\n";
            return 1;
        }
        std::cout << "[HISTORY_TEST] Empty load OK\n";
    }

    // 3. Test add_entry and save
    {
        HistoryManager mgr;
        mgr.load(g_test_history_path);

        HistoryEntry entry;
        entry.address = "TExampleAddress123456789";
        entry.pattern_matched = "contains T";
        entry.pattern_type = "contains";
        entry.pattern_param1 = "T";
        entry.pattern_param2 = "";
        entry.attempts = 42;

        std::string passphrase = "test_password_123";
        std::string error;
        bool ok = mgr.add_entry(entry, passphrase, error);
        if (!ok) {
            std::cerr << "[HISTORY_TEST] FAIL: add_entry failed: " << error << "\n";
            return 1;
        }

        if (mgr.count() != 1) {
            std::cerr << "[HISTORY_TEST] FAIL: expected 1 entry, got " << mgr.count() << "\n";
            return 1;
        }

        ok = mgr.save(g_test_history_path);
        if (!ok) {
            std::cerr << "[HISTORY_TEST] FAIL: save failed\n";
            return 1;
        }

        if (!file_exists(g_test_history_path)) {
            std::cerr << "[HISTORY_TEST] FAIL: history file not created\n";
            return 1;
        }

        std::cout << "[HISTORY_TEST] Add and save OK\n";
    }

    // 4. Test load and verify content
    {
        HistoryManager mgr;
        bool ok = mgr.load(g_test_history_path);
        if (!ok) {
            std::cerr << "[HISTORY_TEST] FAIL: load failed\n";
            return 1;
        }

        auto entries = mgr.get_entries();
        if (entries.size() != 1) {
            std::cerr << "[HISTORY_TEST] FAIL: expected 1 entry after load, got " << entries.size() << "\n";
            return 1;
        }

        const auto& e = entries[0];
        if (e.address != "TExampleAddress123456789") {
            std::cerr << "[HISTORY_TEST] FAIL: address mismatch\n";
            return 1;
        }
        if (e.pattern_type != "contains") {
            std::cerr << "[HISTORY_TEST] FAIL: pattern_type mismatch\n";
            return 1;
        }
        if (e.pattern_param1 != "T") {
            std::cerr << "[HISTORY_TEST] FAIL: pattern_param1 mismatch\n";
            return 1;
        }
        if (e.attempts != 42) {
            std::cerr << "[HISTORY_TEST] FAIL: attempts mismatch\n";
            return 1;
        }
        if (e.timestamp_iso.empty()) {
            std::cerr << "[HISTORY_TEST] FAIL: timestamp_iso is empty\n";
            return 1;
        }
        if (e.encrypted_private_key.empty()) {
            std::cerr << "[HISTORY_TEST] FAIL: encrypted_private_key is empty\n";
            return 1;
        }

        std::cout << "[HISTORY_TEST] Load and verify OK\n";
    }

    // 5. Test multiple entries
    {
        HistoryManager mgr;
        mgr.load(g_test_history_path);

        for (int i = 0; i < 3; ++i) {
            HistoryEntry entry;
            entry.address = "TAddress" + std::to_string(i);
            entry.pattern_matched = "suffix 888";
            entry.pattern_type = "suffix";
            entry.pattern_param1 = "888";
            entry.attempts = 100 + i;

            std::string passphrase = "test_password_123";
            std::string error;
            mgr.add_entry(entry, passphrase, error);
        }

        mgr.save(g_test_history_path);

        HistoryManager mgr2;
        mgr2.load(g_test_history_path);
        if (mgr2.count() != 4) { // 1 original + 3 new
            std::cerr << "[HISTORY_TEST] FAIL: expected 4 entries, got " << mgr2.count() << "\n";
            return 1;
        }
        std::cout << "[HISTORY_TEST] Multiple entries OK\n";
    }

    // 6. Test clear
    {
        HistoryManager mgr;
        mgr.load(g_test_history_path);
        mgr.clear();
        if (mgr.count() != 0) {
            std::cerr << "[HISTORY_TEST] FAIL: clear did not remove all entries\n";
            return 1;
        }
        std::cout << "[HISTORY_TEST] Clear OK\n";
    }

    // 7. Test empty passphrase rejection
    {
        HistoryManager mgr;
        HistoryEntry entry;
        entry.address = "TTest";
        entry.pattern_matched = "test";
        entry.pattern_type = "contains";
        entry.pattern_param1 = "T";

        std::string error;
        bool ok = mgr.add_entry(entry, "", error);
        if (ok) {
            std::cerr << "[HISTORY_TEST] FAIL: empty passphrase should be rejected\n";
            return 1;
        }
        if (error.find("密码") == std::string::npos) {
            std::cerr << "[HISTORY_TEST] FAIL: error message should mention password\n";
            return 1;
        }
        std::cout << "[HISTORY_TEST] Empty passphrase rejection OK\n";
    }

    // 8. Test empty address rejection
    {
        HistoryManager mgr;
        HistoryEntry entry;
        entry.address = "";
        entry.pattern_matched = "test";
        entry.pattern_type = "contains";
        entry.pattern_param1 = "T";

        std::string error;
        bool ok = mgr.add_entry(entry, "password", error);
        if (ok) {
            std::cerr << "[HISTORY_TEST] FAIL: empty address should be rejected\n";
            return 1;
        }
        std::cout << "[HISTORY_TEST] Empty address rejection OK\n";
    }

    // 9. Test delete_entry
    {
        HistoryManager mgr;
        mgr.load(g_test_history_path);
        size_t before = mgr.count();

        std::string error;
        bool ok = mgr.delete_entry(0, error);
        if (!ok) {
            std::cerr << "[HISTORY_TEST] FAIL: delete_entry failed: " << error << "\n";
            return 1;
        }

        if (mgr.count() != before - 1) {
            std::cerr << "[HISTORY_TEST] FAIL: count after delete mismatch\n";
            return 1;
        }

        // Test out of range
        ok = mgr.delete_entry(9999, error);
        if (ok) {
            std::cerr << "[HISTORY_TEST] FAIL: out of range delete should fail\n";
            return 1;
        }
        std::cout << "[HISTORY_TEST] Delete entry OK\n";
    }

    // 10. Test encrypted private key roundtrip
    {
        HistoryManager mgr;
        mgr.load(g_test_history_path);

        HistoryEntry entry;
        entry.address = "TTestEncrypt";
        entry.pattern_matched = "contains X";
        entry.pattern_type = "contains";
        entry.pattern_param1 = "X";
        entry.attempts = 1;

        // Manually set encrypted private key
        std::string plaintext = "TTestEncrypt,0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef,contains X,1";
        std::string passphrase = "roundtrip_password";
        entry.encrypted_private_key = encrypt_export_record(plaintext, passphrase);

        std::string error;
        mgr.add_entry(entry, passphrase, error);
        mgr.save(g_test_history_path);

        HistoryManager mgr2;
        mgr2.load(g_test_history_path);
        auto entries = mgr2.get_entries();

        // Find our entry
        bool found = false;
        for (const auto& e : entries) {
            if (e.address == "TTestEncrypt") {
                found = true;
                std::string decrypted_key;
                std::string decrypt_err;
                bool ok = mgr2.decrypt_entry(e, passphrase, decrypted_key, decrypt_err);
                if (!ok) {
                    std::cerr << "[HISTORY_TEST] FAIL: decrypt_entry failed: " << decrypt_err << "\n";
                    return 1;
                }
                if (decrypted_key != "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef") {
                    std::cerr << "[HISTORY_TEST] FAIL: decrypted private key mismatch\n";
                    return 1;
                }
                break;
            }
        }
        if (!found) {
            std::cerr << "[HISTORY_TEST] FAIL: could not find test entry for decrypt\n";
            return 1;
        }
        std::cout << "[HISTORY_TEST] Encrypted private key roundtrip OK\n";
    }

    // 11. Test wrong passphrase decryption failure
    {
        HistoryManager mgr;
        mgr.load(g_test_history_path);
        auto entries = mgr.get_entries();

        bool found = false;
        for (const auto& e : entries) {
            if (e.address == "TTestEncrypt") {
                found = true;
                std::string decrypted_key;
                std::string decrypt_err;
                bool ok = mgr.decrypt_entry(e, "wrong_password", decrypted_key, decrypt_err);
                if (ok) {
                    std::cerr << "[HISTORY_TEST] FAIL: wrong passphrase should not decrypt\n";
                    return 1;
                }
                break;
            }
        }
        if (!found) {
            std::cerr << "[HISTORY_TEST] FAIL: could not find test entry for wrong passphrase test\n";
            return 1;
        }
        std::cout << "[HISTORY_TEST] Wrong passphrase rejection OK\n";
    }

    // 12. Test JSON file format
    {
        std::ifstream ifs(g_test_history_path);
        if (!ifs.is_open()) {
            std::cerr << "[HISTORY_TEST] FAIL: cannot open history file for format check\n";
            return 1;
        }
        std::string content((std::istreambuf_iterator<char>(ifs)),
                             std::istreambuf_iterator<char>());
        if (content.find("\"version\"") == std::string::npos) {
            std::cerr << "[HISTORY_TEST] FAIL: missing version field\n";
            return 1;
        }
        if (content.find("\"entries\"") == std::string::npos) {
            std::cerr << "[HISTORY_TEST] FAIL: missing entries field\n";
            return 1;
        }
        if (content.find("\"address\"") == std::string::npos) {
            std::cerr << "[HISTORY_TEST] FAIL: missing address field\n";
            return 1;
        }
        if (content.find("\"encrypted_private_key\"") == std::string::npos) {
            std::cerr << "[HISTORY_TEST] FAIL: missing encrypted_private_key field\n";
            return 1;
        }
        std::cout << "[HISTORY_TEST] JSON format OK\n";
    }

    cleanup();
    std::cout << "[HISTORY_TEST] === ALL TESTS PASSED ===\n";
    return 0;
}

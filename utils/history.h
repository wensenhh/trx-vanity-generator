#ifndef TRX_HISTORY_H
#define TRX_HISTORY_H

#include <cstdint>
#include <string>
#include <vector>
#include <chrono>

namespace trx {

// ============================================================================
// History Entry — one saved search session
// ============================================================================

struct HistoryEntry {
    std::string address;
    std::string pattern_matched;
    std::string pattern_type;      // "suffix", "prefix", "contains", etc.
    std::string pattern_param1;
    std::string pattern_param2;
    uint64_t attempts{0};
    std::string timestamp_iso;       // ISO 8601 format
    std::string encrypted_private_key; // AES-256-GCM encrypted

    // For internal use (not serialized)
    std::chrono::system_clock::time_point timestamp;
};

// ============================================================================
// History Manager — save/load/search history records
// ============================================================================

class HistoryManager {
public:
    HistoryManager();
    ~HistoryManager() = default;

    // File path for history storage (default: ~/.trx_vanity/history.json)
    static std::string default_history_path();

    // Load history from file. Returns true on success (empty file is OK).
    bool load(const std::string& filepath = "");

    // Save history to file. Returns true on success.
    bool save(const std::string& filepath = "") const;

    // Add a new entry (encrypts private_key with passphrase)
    bool add_entry(const HistoryEntry& entry,
                   const std::string& passphrase,
                   std::string& error_out);

    // Get all entries (most recent first)
    std::vector<HistoryEntry> get_entries() const;

    // Get entry count
    size_t count() const;

    // Clear all history
    void clear();

    // Decrypt a single entry's private key
    bool decrypt_entry(const HistoryEntry& entry,
                       const std::string& passphrase,
                       std::string& private_key_out,
                       std::string& error_out) const;

    // Delete a specific entry by index
    bool delete_entry(size_t index, std::string& error_out);

private:
    std::vector<HistoryEntry> entries_;
    mutable std::mutex mutex_;

    std::string resolve_path(const std::string& filepath) const;
    bool ensure_directory(const std::string& path) const;
};

} // namespace trx

#endif // TRX_HISTORY_H

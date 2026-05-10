#include "utils/history.h"
#include "utils/export_encryption.h"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <algorithm>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#ifdef _WIN32
#include <direct.h>
#endif

namespace trx {

// ============================================================================
// Helpers
// ============================================================================

static std::string get_home_dir() {
    const char* home = getenv("HOME");
    if (home) return std::string(home);
    return ".";
}

static std::string format_iso_time(std::chrono::system_clock::time_point tp) {
    auto t = std::chrono::system_clock::to_time_t(tp);
    std::tm tm_buf{};
#if defined(_WIN32)
    localtime_s(&tm_buf, &t);
#else
    localtime_r(&t, &tm_buf);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tm_buf, "%Y-%m-%dT%H:%M:%S");
    return oss.str();
}

static std::chrono::system_clock::time_point parse_iso_time(const std::string& iso) {
    std::tm tm_buf{};
    std::istringstream iss(iso);
    iss >> std::get_time(&tm_buf, "%Y-%m-%dT%H:%M:%S");
    if (iss.fail()) {
        return std::chrono::system_clock::now();
    }
    auto t = std::mktime(&tm_buf);
    return std::chrono::system_clock::from_time_t(t);
}

static std::string json_escape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 2);
    for (char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out += c;
                }
        }
    }
    return out;
}

// ============================================================================
// Simple JSON serialization (no external dependency)
// ============================================================================

static std::string serialize_history(const std::vector<HistoryEntry>& entries) {
    std::ostringstream oss;
    oss << "{\n";
    oss << "  \"version\": 1,\n";
    oss << "  \"entries\": [\n";
    for (size_t i = 0; i < entries.size(); ++i) {
        const auto& e = entries[i];
        oss << "    {\n";
        oss << "      \"address\": \"" << json_escape(e.address) << "\",\n";
        oss << "      \"pattern_matched\": \"" << json_escape(e.pattern_matched) << "\",\n";
        oss << "      \"pattern_type\": \"" << json_escape(e.pattern_type) << "\",\n";
        oss << "      \"pattern_param1\": \"" << json_escape(e.pattern_param1) << "\",\n";
        oss << "      \"pattern_param2\": \"" << json_escape(e.pattern_param2) << "\",\n";
        oss << "      \"attempts\": " << e.attempts << ",\n";
        oss << "      \"timestamp\": \"" << json_escape(e.timestamp_iso) << "\",\n";
        oss << "      \"encrypted_private_key\": \"" << json_escape(e.encrypted_private_key) << "\"\n";
        oss << "    }";
        if (i + 1 < entries.size()) oss << ",";
        oss << "\n";
    }
    oss << "  ]\n";
    oss << "}\n";
    return oss.str();
}

static bool parse_json_string(const std::string& json, size_t& pos,
                               const std::string& key, std::string& out) {
    size_t key_pos = json.find("\"" + key + "\"", pos);
    if (key_pos == std::string::npos) return false;
    size_t colon = json.find(':', key_pos);
    if (colon == std::string::npos) return false;
    size_t quote1 = json.find('"', colon);
    if (quote1 == std::string::npos) return false;
    size_t quote2 = json.find('"', quote1 + 1);
    if (quote2 == std::string::npos) return false;
    // Handle escapes
    out.clear();
    for (size_t i = quote1 + 1; i < quote2; ++i) {
        if (json[i] == '\\' && i + 1 < quote2) {
            char next = json[i + 1];
            switch (next) {
                case '"': out += '"'; ++i; break;
                case '\\': out += '\\'; ++i; break;
                case 'b': out += '\b'; ++i; break;
                case 'f': out += '\f'; ++i; break;
                case 'n': out += '\n'; ++i; break;
                case 'r': out += '\r'; ++i; break;
                case 't': out += '\t'; ++i; break;
                case 'u': {
                    if (i + 5 < quote2) {
                        std::string hex = json.substr(i + 2, 4);
                        char code = static_cast<char>(std::stoi(hex, nullptr, 16));
                        out += code;
                        i += 5;
                    }
                    break;
                }
                default: out += next; ++i; break;
            }
        } else {
            out += json[i];
        }
    }
    pos = quote2 + 1;
    return true;
}

static bool parse_json_uint64(const std::string& json, size_t& pos,
                               const std::string& key, uint64_t& out) {
    size_t key_pos = json.find("\"" + key + "\"", pos);
    if (key_pos == std::string::npos) return false;
    size_t colon = json.find(':', key_pos);
    if (colon == std::string::npos) return false;
    size_t start = colon + 1;
    while (start < json.size() && isspace(json[start])) ++start;
    size_t end = start;
    while (end < json.size() && (isdigit(json[end]))) ++end;
    if (start == end) return false;
    out = std::stoull(json.substr(start, end - start));
    pos = end;
    return true;
}

static std::vector<HistoryEntry> deserialize_history(const std::string& json) {
    std::vector<HistoryEntry> entries;
    size_t pos = 0;
    // Find "entries" array
    size_t arr_start = json.find("\"entries\"", pos);
    if (arr_start == std::string::npos) return entries;
    size_t bracket = json.find('[', arr_start);
    if (bracket == std::string::npos) return entries;
    
    size_t i = bracket + 1;
    int brace_depth = 0;
    bool in_string = false;
    bool in_entry = false;
    
    while (i < json.size()) {
        char c = json[i];
        if (c == '"' && (i == 0 || json[i-1] != '\\')) {
            in_string = !in_string;
        } else if (!in_string) {
            if (c == '{') {
                if (brace_depth == 0) {
                    in_entry = true;
                    HistoryEntry e;
                    size_t entry_start = i;
                    // Find matching }
                    size_t j = i + 1;
                    int depth = 1;
                    bool j_in_string = false;
                    while (j < json.size() && depth > 0) {
                        if (json[j] == '"' && (j == 0 || json[j-1] != '\\')) {
                            j_in_string = !j_in_string;
                        } else if (!j_in_string) {
                            if (json[j] == '{') ++depth;
                            else if (json[j] == '}') --depth;
                        }
                        ++j;
                    }
                    std::string entry_json = json.substr(entry_start, j - entry_start);
                    size_t ep = 0;
                    parse_json_string(entry_json, ep, "address", e.address);
                    ep = 0;
                    parse_json_string(entry_json, ep, "pattern_matched", e.pattern_matched);
                    ep = 0;
                    parse_json_string(entry_json, ep, "pattern_type", e.pattern_type);
                    ep = 0;
                    parse_json_string(entry_json, ep, "pattern_param1", e.pattern_param1);
                    ep = 0;
                    parse_json_string(entry_json, ep, "pattern_param2", e.pattern_param2);
                    ep = 0;
                    parse_json_uint64(entry_json, ep, "attempts", e.attempts);
                    ep = 0;
                    parse_json_string(entry_json, ep, "timestamp", e.timestamp_iso);
                    ep = 0;
                    parse_json_string(entry_json, ep, "encrypted_private_key", e.encrypted_private_key);
                    e.timestamp = parse_iso_time(e.timestamp_iso);
                    entries.push_back(e);
                    i = j;
                    in_entry = false;
                    continue;
                }
                ++brace_depth;
            } else if (c == '}') {
                --brace_depth;
            } else if (c == ']' && brace_depth == 0) {
                break;
            }
        }
        ++i;
    }
    return entries;
}

// ============================================================================
// HistoryManager Implementation
// ============================================================================

HistoryManager::HistoryManager() = default;

std::string HistoryManager::default_history_path() {
    return get_home_dir() + "/.trx_vanity/history.json";
}

std::string HistoryManager::resolve_path(const std::string& filepath) const {
    if (filepath.empty()) {
        return default_history_path();
    }
    return filepath;
}

bool HistoryManager::ensure_directory(const std::string& path) const {
    size_t last_slash = path.find_last_of("/\\");
    if (last_slash == std::string::npos) return true;
    std::string dir = path.substr(0, last_slash);
    if (dir.empty() || dir == ".") return true;
    
    struct stat st;
    if (stat(dir.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) {
        return true;
    }
    
#ifdef _WIN32
    (void)_mkdir(dir.c_str());
#else
    (void)mkdir(dir.c_str(), 0755);
#endif
    return true;
}

bool HistoryManager::load(const std::string& filepath) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string path = resolve_path(filepath);
    
    std::ifstream ifs(path);
    if (!ifs.is_open()) {
        // No history file yet — that's OK
        entries_.clear();
        return true;
    }
    
    std::string content((std::istreambuf_iterator<char>(ifs)),
                         std::istreambuf_iterator<char>());
    if (content.empty()) {
        entries_.clear();
        return true;
    }
    
    entries_ = deserialize_history(content);
    // Sort by timestamp descending (most recent first)
    std::sort(entries_.begin(), entries_.end(),
              [](const HistoryEntry& a, const HistoryEntry& b) {
                  return a.timestamp > b.timestamp;
              });
    return true;
}

bool HistoryManager::save(const std::string& filepath) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string path = resolve_path(filepath);
    
    if (!ensure_directory(path)) {
        return false;
    }
    
    std::ofstream ofs(path);
    if (!ofs.is_open()) {
        return false;
    }
    
    ofs << serialize_history(entries_);
    return ofs.good();
}

bool HistoryManager::add_entry(const HistoryEntry& entry,
                                const std::string& passphrase,
                                std::string& error_out) {
    error_out.clear();
    if (passphrase.empty()) {
        error_out = "密码不能为空";
        return false;
    }
    if (entry.address.empty()) {
        error_out = "地址不能为空";
        return false;
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    HistoryEntry e = entry;
    e.timestamp = std::chrono::system_clock::now();
    e.timestamp_iso = format_iso_time(e.timestamp);
    if (e.encrypted_private_key.empty()) {
        // Encrypt the private key if not already encrypted
        std::string plaintext = e.address + "," + 
                                e.pattern_matched + "," +
                                std::to_string(e.attempts);
        try {
            e.encrypted_private_key = encrypt_export_record(plaintext, passphrase);
        } catch (const std::exception& ex) {
            error_out = std::string("加密失败: ") + ex.what();
            return false;
        }
    }
    entries_.insert(entries_.begin(), e);
    return true;
}

std::vector<HistoryEntry> HistoryManager::get_entries() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return entries_;
}

size_t HistoryManager::count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return entries_.size();
}

void HistoryManager::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    entries_.clear();
}

bool HistoryManager::decrypt_entry(const HistoryEntry& entry,
                                    const std::string& passphrase,
                                    std::string& private_key_out,
                                    std::string& error_out) const {
    error_out.clear();
    private_key_out.clear();
    if (passphrase.empty()) {
        error_out = "密码不能为空";
        return false;
    }
    if (entry.encrypted_private_key.empty()) {
        error_out = "没有加密的私钥";
        return false;
    }
    
    std::string plaintext;
    if (!decrypt_export_record(entry.encrypted_private_key, passphrase, plaintext)) {
        error_out = "解密失败：密码错误或数据损坏";
        return false;
    }
    
    // plaintext format: address,private_key_hex,pattern_matched,attempts
    // Extract private key (second field)
    size_t first_comma = plaintext.find(',');
    if (first_comma == std::string::npos) {
        error_out = "解密数据格式错误";
        return false;
    }
    size_t second_comma = plaintext.find(',', first_comma + 1);
    if (second_comma == std::string::npos) {
        error_out = "解密数据格式错误";
        return false;
    }
    private_key_out = plaintext.substr(first_comma + 1, second_comma - first_comma - 1);
    return true;
}

bool HistoryManager::delete_entry(size_t index, std::string& error_out) {
    error_out.clear();
    std::lock_guard<std::mutex> lock(mutex_);
    if (index >= entries_.size()) {
        error_out = "索引超出范围";
        return false;
    }
    entries_.erase(entries_.begin() + index);
    return true;
}

} // namespace trx

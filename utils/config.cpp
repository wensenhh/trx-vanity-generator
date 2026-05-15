#include "utils/config.h"
#include "utils/constants.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <cstdlib>
#include <algorithm>

namespace trx {

// ============================================================================
// JSON helpers (minimal, no external dependency)
// ============================================================================

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
            default: out += c; break;
        }
    }
    return out;
}

static std::string json_string_value(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\"";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return "";
    pos = json.find(':', pos + search.size());
    if (pos == std::string::npos) return "";
    pos = json.find('"', pos);
    if (pos == std::string::npos) return "";
    ++pos;
    size_t end = json.find('"', pos);
    if (end == std::string::npos) return "";
    // Handle escaped quotes
    std::string result;
    for (size_t i = pos; i < end; ++i) {
        if (json[i] == '\\' && i + 1 < end) {
            char next = json[i + 1];
            switch (next) {
                case '"': result += '"'; ++i; break;
                case '\\': result += '\\'; ++i; break;
                case '/': result += '/'; ++i; break;
                case 'b': result += '\b'; ++i; break;
                case 'f': result += '\f'; ++i; break;
                case 'n': result += '\n'; ++i; break;
                case 'r': result += '\r'; ++i; break;
                case 't': result += '\t'; ++i; break;
                default: result += json[i]; break;
            }
        } else {
            result += json[i];
        }
    }
    return result;
}

static std::optional<bool> json_bool_value(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\"";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return std::nullopt;
    pos = json.find(':', pos + search.size());
    if (pos == std::string::npos) return std::nullopt;
    ++pos;
    while (pos < json.size() && std::isspace(static_cast<unsigned char>(json[pos]))) ++pos;
    if (pos >= json.size()) return std::nullopt;
    std::string val;
    while (pos < json.size() && !std::isspace(static_cast<unsigned char>(json[pos])) && json[pos] != ',' && json[pos] != '}' && json[pos] != ']') {
        val += json[pos++];
    }
    if (val == "true") return true;
    if (val == "false") return false;
    return std::nullopt;
}

static std::optional<int> json_int_value(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\"";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return std::nullopt;
    pos = json.find(':', pos + search.size());
    if (pos == std::string::npos) return std::nullopt;
    ++pos;
    while (pos < json.size() && std::isspace(static_cast<unsigned char>(json[pos]))) ++pos;
    if (pos >= json.size()) return std::nullopt;
    try {
        size_t end = 0;
        int val = std::stoi(json.substr(pos), &end);
        return val;
    } catch (...) {
        return std::nullopt;
    }
}

static std::optional<size_t> json_size_value(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\"";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return std::nullopt;
    pos = json.find(':', pos + search.size());
    if (pos == std::string::npos) return std::nullopt;
    ++pos;
    while (pos < json.size() && std::isspace(static_cast<unsigned char>(json[pos]))) ++pos;
    if (pos >= json.size()) return std::nullopt;
    try {
        size_t end = 0;
        unsigned long long val = std::stoull(json.substr(pos), &end, 10);
        return static_cast<size_t>(val);
    } catch (...) {
        return std::nullopt;
    }
}

static std::optional<uint64_t> json_u64_value(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\"";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return std::nullopt;
    pos = json.find(':', pos + search.size());
    if (pos == std::string::npos) return std::nullopt;
    ++pos;
    while (pos < json.size() && std::isspace(static_cast<unsigned char>(json[pos]))) ++pos;
    if (pos >= json.size()) return std::nullopt;
    try {
        size_t end = 0;
        unsigned long long val = std::stoull(json.substr(pos), &end, 10);
        return static_cast<uint64_t>(val);
    } catch (...) {
        return std::nullopt;
    }
}

// ============================================================================
// ConfigManager implementation
// ============================================================================

std::string ConfigManager::default_config_path() {
    const char* home = std::getenv("HOME");
    if (!home || home[0] == '\0') {
        home = ".";
    }
    return std::string(home) + "/.trx_vanity_config.json";
}

bool ConfigManager::load(const std::string& filepath, CLIConfig& out, std::string& error) {
    std::ifstream ifs(filepath);
    if (!ifs.is_open()) {
        error = "Cannot open config file: " + filepath;
        return false;
    }
    std::stringstream ss;
    ss << ifs.rdbuf();
    std::string json = ss.str();
    ifs.close();

    if (json.empty()) {
        error = "Config file is empty: " + filepath;
        return false;
    }

    // Parse defaults
    out.gpu = json_bool_value(json, "gpu");
    out.threads = json_int_value(json, "threads");
    out.batch_size = json_size_value(json, "batch_size");
    out.max_attempts = json_u64_value(json, "max_attempts");
    out.verbose = json_bool_value(json, "verbose");
    out.output = json_string_value(json, "output");
    out.encrypted_output = json_string_value(json, "encrypted_output");
    out.export_password_env = json_string_value(json, "export_password_env");
    out.show_private_key = json_bool_value(json, "show_private_key");

    // Parse tasks array
    std::string tasks_search = "\"tasks\"";
    size_t tasks_pos = json.find(tasks_search);
    if (tasks_pos != std::string::npos) {
        size_t bracket = json.find('[', tasks_pos);
        if (bracket != std::string::npos) {
            size_t depth = 1;
            size_t i = bracket + 1;
            std::string current_task;
            while (i < json.size() && depth > 0) {
                char c = json[i];
                if (c == '"') {
                    // Find the end of this string (handle escapes)
                    size_t str_start = i;
                    ++i;
                    while (i < json.size()) {
                        if (json[i] == '"' && json[i-1] != '\\') break;
                        ++i;
                    }
                    if (i < json.size()) ++i; // skip closing quote
                    // Append the entire string including quotes
                    if (depth >= 2) current_task += json.substr(str_start, i - str_start);
                    continue;
                }
                if (c == '[') {
                    ++depth;
                    if (depth >= 2) current_task += c;
                } else if (c == ']') {
                    --depth;
                    if (depth >= 2) current_task += c;
                } else if (c == '{') {
                    if (depth == 1) current_task.clear();
                    ++depth;
                    if (depth >= 2) current_task += c;
                } else if (c == '}') {
                    --depth;
                    if (depth >= 2) current_task += c;
                    if (depth == 1) {
                        // Complete task object — parse the collected content
                        TaskConfig task;
                        task.pattern_type = json_string_value(current_task, "pattern_type");
                        task.pattern_arg = json_string_value(current_task, "pattern");
                        task.pattern_arg2 = json_string_value(current_task, "pattern_arg2");
                        task.gpu = json_bool_value(current_task, "gpu");
                        task.threads = json_int_value(current_task, "threads");
                        task.batch_size = json_size_value(current_task, "batch_size");
                        task.max_attempts = json_u64_value(current_task, "max_attempts");
                        task.output = json_string_value(current_task, "output");
                        task.verbose = json_bool_value(current_task, "verbose");
                        task.encrypted_output = json_string_value(current_task, "encrypted_output");
                        task.export_password_env = json_string_value(current_task, "export_password_env");
                        task.show_private_key = json_bool_value(current_task, "show_private_key");
                        out.tasks.push_back(task);
                        current_task.clear();
                    }
                } else {
                    if (depth >= 2) current_task += c;
                }
                ++i;
            }
        }
    }

    // If no tasks array but has pattern_type at top level, treat as single task
    if (out.tasks.empty()) {
        std::string pt = json_string_value(json, "pattern_type");
        if (!pt.empty()) {
            TaskConfig task;
            task.pattern_type = pt;
            task.pattern_arg = json_string_value(json, "pattern");
            task.pattern_arg2 = json_string_value(json, "pattern_arg2");
            task.gpu = json_bool_value(json, "gpu");
            task.threads = json_int_value(json, "threads");
            task.batch_size = json_size_value(json, "batch_size");
            task.max_attempts = json_u64_value(json, "max_attempts");
            task.output = json_string_value(json, "output");
            task.verbose = json_bool_value(json, "verbose");
            task.encrypted_output = json_string_value(json, "encrypted_output");
            task.export_password_env = json_string_value(json, "export_password_env");
            task.show_private_key = json_bool_value(json, "show_private_key");
            out.tasks.push_back(task);
        }
    }

    return true;
}

bool ConfigManager::save(const std::string& filepath, const CLIConfig& config, std::string& error) {
    std::ofstream ofs(filepath);
    if (!ofs.is_open()) {
        error = "Cannot write config file: " + filepath;
        return false;
    }

    ofs << "{\n";
    bool first = true;
    auto comma = [&]() {
        if (!first) ofs << ",\n";
        first = false;
    };

    if (config.gpu.has_value()) {
        comma();
        ofs << "  \"gpu\": " << (config.gpu.value() ? "true" : "false");
    }
    if (config.threads.has_value()) {
        comma();
        ofs << "  \"threads\": " << config.threads.value();
    }
    if (config.batch_size.has_value()) {
        comma();
        ofs << "  \"batch_size\": " << config.batch_size.value();
    }
    if (config.max_attempts.has_value()) {
        comma();
        ofs << "  \"max_attempts\": " << config.max_attempts.value();
    }
    if (config.verbose.has_value()) {
        comma();
        ofs << "  \"verbose\": " << (config.verbose.value() ? "true" : "false");
    }
    if (config.output.has_value()) {
        comma();
        ofs << "  \"output\": \"" << json_escape(config.output.value()) << "\"";
    }
    if (config.encrypted_output.has_value()) {
        comma();
        ofs << "  \"encrypted_output\": \"" << json_escape(config.encrypted_output.value()) << "\"";
    }
    if (config.export_password_env.has_value()) {
        comma();
        ofs << "  \"export_password_env\": \"" << json_escape(config.export_password_env.value()) << "\"";
    }
    if (config.show_private_key.has_value()) {
        comma();
        ofs << "  \"show_private_key\": " << (config.show_private_key.value() ? "true" : "false");
    }

    if (!config.tasks.empty()) {
        comma();
        ofs << "  \"tasks\": [\n";
        for (size_t i = 0; i < config.tasks.size(); ++i) {
            const auto& t = config.tasks[i];
            ofs << "    {\n";
            ofs << "      \"pattern_type\": \"" << json_escape(t.pattern_type) << "\"";
            if (!t.pattern_arg.empty()) {
                ofs << ",\n      \"pattern\": \"" << json_escape(t.pattern_arg) << "\"";
            }
            if (!t.pattern_arg2.empty()) {
                ofs << ",\n      \"pattern_arg2\": \"" << json_escape(t.pattern_arg2) << "\"";
            }
            if (t.gpu.has_value()) {
                ofs << ",\n      \"gpu\": " << (t.gpu.value() ? "true" : "false");
            }
            if (t.threads.has_value()) {
                ofs << ",\n      \"threads\": " << t.threads.value();
            }
            if (t.batch_size.has_value()) {
                ofs << ",\n      \"batch_size\": " << t.batch_size.value();
            }
            if (t.max_attempts.has_value()) {
                ofs << ",\n      \"max_attempts\": " << t.max_attempts.value();
            }
            if (t.output.has_value()) {
                ofs << ",\n      \"output\": \"" << json_escape(t.output.value()) << "\"";
            }
            if (t.verbose.has_value()) {
                ofs << ",\n      \"verbose\": " << (t.verbose.value() ? "true" : "false");
            }
            if (t.encrypted_output.has_value()) {
                ofs << ",\n      \"encrypted_output\": \"" << json_escape(t.encrypted_output.value()) << "\"";
            }
            if (t.export_password_env.has_value()) {
                ofs << ",\n      \"export_password_env\": \"" << json_escape(t.export_password_env.value()) << "\"";
            }
            if (t.show_private_key.has_value()) {
                ofs << ",\n      \"show_private_key\": " << (t.show_private_key.value() ? "true" : "false");
            }
            ofs << "\n    }";
            if (i + 1 < config.tasks.size()) ofs << ",";
            ofs << "\n";
        }
        ofs << "  ]";
    }

    ofs << "\n}\n";
    ofs.close();
    return true;
}

CLIConfig ConfigManager::merge_with_cli(const CLIConfig& config, const TaskConfig& cli_overrides) {
    CLIConfig merged = config;

    // CLI overrides take precedence over file defaults
    if (cli_overrides.gpu.has_value()) merged.gpu = cli_overrides.gpu;
    if (cli_overrides.threads.has_value()) merged.threads = cli_overrides.threads;
    if (cli_overrides.batch_size.has_value()) merged.batch_size = cli_overrides.batch_size;
    if (cli_overrides.max_attempts.has_value()) merged.max_attempts = cli_overrides.max_attempts;
    if (cli_overrides.verbose.has_value()) merged.verbose = cli_overrides.verbose;
    if (cli_overrides.output.has_value()) merged.output = cli_overrides.output;
    if (cli_overrides.encrypted_output.has_value()) merged.encrypted_output = cli_overrides.encrypted_output;
    if (cli_overrides.export_password_env.has_value()) merged.export_password_env = cli_overrides.export_password_env;
    if (cli_overrides.show_private_key.has_value()) merged.show_private_key = cli_overrides.show_private_key;

    return merged;
}

int ConfigManager::validate_task(const TaskConfig& task, std::string& error) {
    if (task.pattern_type.empty()) {
        error = "pattern_type is required";
        return 2;
    }
    static const char* valid_types[] = {"prefix", "suffix", "contains", "consecutive", "sequential"};
    bool valid = false;
    for (const auto* t : valid_types) {
        if (task.pattern_type == t) { valid = true; break; }
    }
    if (!valid) {
        error = "unknown pattern_type: " + task.pattern_type;
        return 2;
    }
    if (task.pattern_arg.empty()) {
        error = "pattern (pattern_arg) is required";
        return 2;
    }
    return 0;
}

} // namespace trx

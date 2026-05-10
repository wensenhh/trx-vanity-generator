#pragma once

#include <string>
#include <vector>
#include <optional>
#include <cstdint>

namespace trx {

// ============================================================================
// Single search task configuration
// ============================================================================
struct TaskConfig {
    std::string pattern_type;   // "prefix", "suffix", "contains", "consecutive", "sequential"
    std::string pattern_arg;    // main pattern argument
    std::string pattern_arg2;   // optional second argument (length for consecutive/sequential)
    std::optional<bool> gpu;
    std::optional<int> threads;
    std::optional<size_t> batch_size;
    std::optional<uint64_t> max_attempts;
    std::optional<std::string> output;
    std::optional<bool> verbose;
    std::optional<std::string> encrypted_output;
    std::optional<std::string> export_password_env;
    std::optional<bool> show_private_key;

    bool empty() const {
        return pattern_type.empty() && pattern_arg.empty();
    }
};

// ============================================================================
// Global CLI configuration
// ============================================================================
struct CLIConfig {
    // Default settings (used when not specified by task or command line)
    std::optional<bool> gpu;
    std::optional<int> threads;
    std::optional<size_t> batch_size;
    std::optional<uint64_t> max_attempts;
    std::optional<bool> verbose;
    std::optional<std::string> output;
    std::optional<std::string> encrypted_output;
    std::optional<std::string> export_password_env;
    std::optional<bool> show_private_key;

    // Batch tasks
    std::vector<TaskConfig> tasks;

    bool has_defaults() const {
        return gpu.has_value() || threads.has_value() || batch_size.has_value() ||
               max_attempts.has_value() || verbose.has_value() || output.has_value();
    }
};

// ============================================================================
// Config I/O
// ============================================================================
class ConfigManager {
public:
    // Load config from JSON file. Returns true on success.
    static bool load(const std::string& filepath, CLIConfig& out, std::string& error);

    // Save config to JSON file. Returns true on success.
    static bool save(const std::string& filepath, const CLIConfig& config, std::string& error);

    // Default config file path: ~/.trx_vanity_config.json
    static std::string default_config_path();

    // Merge command-line overrides into config (CLI takes precedence)
    static CLIConfig merge_with_cli(const CLIConfig& config,
                                     const TaskConfig& cli_overrides);

    // Validate a task config. Returns 0 on success, error code otherwise.
    static int validate_task(const TaskConfig& task, std::string& error);
};

} // namespace trx

#include "utils/config.h"
#include <cassert>
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstdlib>

using namespace trx;

static std::string g_test_config_path = "/tmp/test_trx_vanity_config.json";

static void cleanup() {
    std::remove(g_test_config_path.c_str());
}

static bool test_save_load_basic() {
    cleanup();
    CLIConfig config;
    config.gpu = true;
    config.threads = 4;
    config.batch_size = 65536;
    config.max_attempts = 1000000;
    config.verbose = true;
    config.output = "results.txt";

    std::string error;
    bool ok = ConfigManager::save(g_test_config_path, config, error);
    if (!ok) {
        std::cerr << "FAIL: save basic: " << error << "\n";
        return false;
    }

    CLIConfig loaded;
    ok = ConfigManager::load(g_test_config_path, loaded, error);
    if (!ok) {
        std::cerr << "FAIL: load basic: " << error << "\n";
        return false;
    }

    assert(loaded.gpu.has_value() && loaded.gpu.value() == true);
    assert(loaded.threads.has_value() && loaded.threads.value() == 4);
    assert(loaded.batch_size.has_value() && loaded.batch_size.value() == 65536);
    assert(loaded.max_attempts.has_value() && loaded.max_attempts.value() == 1000000);
    assert(loaded.verbose.has_value() && loaded.verbose.value() == true);
    assert(loaded.output.has_value() && loaded.output.value() == "results.txt");
    assert(loaded.tasks.empty());

    std::cout << "PASS: save_load_basic\n";
    cleanup();
    return true;
}

static bool test_save_load_tasks() {
    cleanup();
    CLIConfig config;
    config.gpu = false;
    config.threads = 2;

    TaskConfig t1;
    t1.pattern_type = "suffix";
    t1.pattern_arg = "8888";
    t1.gpu = true;
    t1.max_attempts = 500000;
    config.tasks.push_back(t1);

    TaskConfig t2;
    t2.pattern_type = "prefix";
    t2.pattern_arg = "ABC";
    t2.threads = 8;
    config.tasks.push_back(t2);

    std::string error;
    bool ok = ConfigManager::save(g_test_config_path, config, error);
    if (!ok) {
        std::cerr << "FAIL: save tasks: " << error << "\n";
        return false;
    }

    // Debug: print file content
    {
        std::ifstream ifs(g_test_config_path);
        std::stringstream ss;
        ss << ifs.rdbuf();
        std::cout << "DEBUG config file:\n" << ss.str() << "\n";
    }

    CLIConfig loaded;
    ok = ConfigManager::load(g_test_config_path, loaded, error);
    if (!ok) {
        std::cerr << "FAIL: load tasks: " << error << "\n";
        return false;
    }

    std::cout << "DEBUG loaded tasks count: " << loaded.tasks.size() << "\n";
    for (size_t i = 0; i < loaded.tasks.size(); ++i) {
        std::cout << "DEBUG task[" << i << "]: type=" << loaded.tasks[i].pattern_type
                  << " arg=" << loaded.tasks[i].pattern_arg << "\n";
    }

    assert(loaded.tasks.size() == 2);
    // Tasks may be loaded in any order depending on JSON parsing; just verify both exist
    bool found_suffix = false, found_prefix = false;
    for (const auto& t : loaded.tasks) {
        if (t.pattern_type == "suffix") {
            found_suffix = true;
            assert(t.pattern_arg == "8888");
            assert(t.gpu.has_value() && t.gpu.value() == true);
            assert(t.max_attempts.has_value() && t.max_attempts.value() == 500000);
        } else if (t.pattern_type == "prefix") {
            found_prefix = true;
            assert(t.pattern_arg == "ABC");
            assert(t.threads.has_value() && t.threads.value() == 8);
        }
    }
    assert(found_suffix);
    assert(found_prefix);

    std::cout << "PASS: save_load_tasks\n";
    cleanup();
    return true;
}

static bool test_merge_with_cli() {
    CLIConfig config;
    config.gpu = false;
    config.threads = 2;
    config.batch_size = 32768;

    TaskConfig cli;
    cli.gpu = true;
    cli.verbose = true;

    CLIConfig merged = ConfigManager::merge_with_cli(config, cli);

    assert(merged.gpu.has_value() && merged.gpu.value() == true);  // CLI override
    assert(merged.threads.has_value() && merged.threads.value() == 2);  // from file
    assert(merged.batch_size.has_value() && merged.batch_size.value() == 32768);  // from file
    assert(merged.verbose.has_value() && merged.verbose.value() == true);  // CLI override

    std::cout << "PASS: merge_with_cli\n";
    return true;
}

static bool test_validate_task() {
    std::string error;

    TaskConfig valid;
    valid.pattern_type = "suffix";
    valid.pattern_arg = "8888";
    assert(ConfigManager::validate_task(valid, error) == 0);

    TaskConfig invalid_type;
    invalid_type.pattern_type = "invalid";
    invalid_type.pattern_arg = "x";
    assert(ConfigManager::validate_task(invalid_type, error) == 2);

    TaskConfig missing_pattern;
    missing_pattern.pattern_type = "prefix";
    assert(ConfigManager::validate_task(missing_pattern, error) == 2);

    std::cout << "PASS: validate_task\n";
    return true;
}

static bool test_default_config_path() {
    std::string path = ConfigManager::default_config_path();
    assert(path.find(".trx_vanity_config.json") != std::string::npos);
    std::cout << "PASS: default_config_path: " << path << "\n";
    return true;
}

static bool test_single_task_top_level() {
    cleanup();
    std::ofstream ofs(g_test_config_path);
    ofs << "{\n"
        << "  \"pattern_type\": \"contains\",\n"
        << "  \"pattern\": \"T\",\n"
        << "  \"max_attempts\": 1000,\n"
        << "  \"threads\": 1\n"
        << "}\n";
    ofs.close();

    CLIConfig loaded;
    std::string error;
    bool ok = ConfigManager::load(g_test_config_path, loaded, error);
    if (!ok) {
        std::cerr << "FAIL: load single task: " << error << "\n";
        return false;
    }

    assert(loaded.tasks.size() == 1);
    assert(loaded.tasks[0].pattern_type == "contains");
    assert(loaded.tasks[0].pattern_arg == "T");
    assert(loaded.tasks[0].max_attempts.has_value() && loaded.tasks[0].max_attempts.value() == 1000);
    assert(loaded.tasks[0].threads.has_value() && loaded.tasks[0].threads.value() == 1);

    std::cout << "PASS: single_task_top_level\n";
    cleanup();
    return true;
}

int main() {
    bool all_pass = true;
    all_pass &= test_save_load_basic();
    all_pass &= test_save_load_tasks();
    all_pass &= test_merge_with_cli();
    all_pass &= test_validate_task();
    all_pass &= test_default_config_path();
    all_pass &= test_single_task_top_level();

    if (all_pass) {
        std::cout << "\nAll config tests passed!\n";
        return 0;
    }
    std::cerr << "\nSome config tests FAILED!\n";
    return 1;
}

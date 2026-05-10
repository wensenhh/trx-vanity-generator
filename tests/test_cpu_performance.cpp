#include "host/cpu_generator.h"
#include "utils/pattern.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <cassert>
#include <cmath>

using namespace trx;

// ============================================================================
// CPU Performance / Adaptive Tuning Test
// ============================================================================

static void log(const std::string& msg) {
    std::cout << "[CPU_PERF_TEST] " << msg << "\n";
}

static bool approx_equal(double a, double b, double tolerance = 0.01) {
    return std::fabs(a - b) < tolerance;
}

int main(int argc, char** argv) {
    (void)argc; (void)argv;

    log("=== CPU Performance Test Start ===");

    // ------------------------------------------------------------------------
    // 1. Test detect_optimal_threads returns a sane value
    // ------------------------------------------------------------------------
    {
        CPUGenerator gen;
        size_t optimal = gen.detect_optimal_threads();
        log("detect_optimal_threads() = " + std::to_string(optimal));
        assert(optimal >= 1);
        assert(optimal <= std::thread::hardware_concurrency());
        log("optimal threads sanity check PASSED");
    }

    // ------------------------------------------------------------------------
    // 2. Test detect_optimal_batch_size returns a sane value
    // ------------------------------------------------------------------------
    {
        CPUGenerator gen;
        size_t optimal = gen.detect_optimal_batch_size();
        log("detect_optimal_batch_size() = " + std::to_string(optimal));
        assert(optimal >= 1000);
        assert(optimal <= 50000);
        log("optimal batch size sanity check PASSED");
    }

    // ------------------------------------------------------------------------
    // 3. Test apply_auto_tune with adaptive mode ON
    // ------------------------------------------------------------------------
    {
        CPUGenerator gen;
        gen.set_adaptive_threads(true);
        gen.set_adaptive_batch_size(true);
        gen.apply_auto_tune();
        log("apply_auto_tune (adaptive ON) done");
        // After apply_auto_tune, the internal values should be set
        // We verify by starting a tiny run
        auto pattern = PatternFactory::create(PatternType::CONTAINS, "T");
        gen.set_pattern(std::move(pattern));
        gen.set_max_attempts(100);
        gen.start();
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        gen.stop();
        log("tiny run after auto_tune completed");
    }

    // ------------------------------------------------------------------------
    // 4. Test apply_auto_tune with adaptive mode OFF (manual override)
    // ------------------------------------------------------------------------
    {
        CPUGenerator gen;
        gen.set_num_threads(2);
        gen.set_batch_size(500);
        gen.set_adaptive_threads(false);
        gen.set_adaptive_batch_size(false);
        gen.apply_auto_tune(); // should NOT override manual values
        log("apply_auto_tune (adaptive OFF) done");

        auto pattern = PatternFactory::create(PatternType::CONTAINS, "T");
        gen.set_pattern(std::move(pattern));
        gen.set_max_attempts(100);
        gen.start();
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        gen.stop();
        log("tiny run with manual settings completed");
    }

    // ------------------------------------------------------------------------
    // 5. Smoke test with different batch sizes
    // ------------------------------------------------------------------------
    std::vector<size_t> batch_sizes = {100, 1000, 5000, 10000};
    for (size_t bs : batch_sizes) {
        CPUGenerator gen;
        gen.set_batch_size(bs);
        gen.set_num_threads(1);
        gen.set_adaptive_batch_size(false);
        gen.set_adaptive_threads(false);

        auto pattern = PatternFactory::create(PatternType::CONTAINS, "T");
        gen.set_pattern(std::move(pattern));
        gen.set_max_attempts(500);

        auto t0 = std::chrono::steady_clock::now();
        gen.start();
        while (gen.is_running()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        auto t1 = std::chrono::steady_clock::now();
        double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
        log("batch_size=" + std::to_string(bs) + " -> " +
            std::to_string(gen.get_total_attempts()) + " attempts in " +
            std::to_string(static_cast<int>(ms)) + " ms");
    }
    log("batch size smoke tests PASSED");

    // ------------------------------------------------------------------------
    // 6. Rate measurement sanity check
    // ------------------------------------------------------------------------
    {
        CPUGenerator gen;
        gen.set_num_threads(1);
        gen.set_batch_size(1000);
        gen.set_adaptive_batch_size(false);
        gen.set_adaptive_threads(false);

        auto pattern = PatternFactory::create(PatternType::CONTAINS, "T");
        gen.set_pattern(std::move(pattern));
        gen.set_max_attempts(2000);

        gen.start();
        while (gen.is_running()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        double rate = gen.get_rate();
        log("rate measurement = " + std::to_string(static_cast<int>(rate)) + " addr/s");
        assert(rate > 0.0); // should have generated something
        log("rate sanity check PASSED");
    }

    // ------------------------------------------------------------------------
    // 7. Test adaptive flags survive set_pattern recreation
    // ------------------------------------------------------------------------
    {
        CPUGenerator gen;
        gen.set_adaptive_threads(true);
        gen.set_adaptive_batch_size(true);

        auto pattern = PatternFactory::create(PatternType::CONTAINS, "T");
        gen.set_pattern(std::move(pattern));

        assert(gen.is_adaptive_batch_size() == true);
        assert(gen.is_adaptive_threads() == true);
        log("adaptive flags survive set_pattern PASSED");
    }

    log("=== CPU Performance Test PASSED ===");
    return 0;
}

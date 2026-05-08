#include "utils/estimate.h"

#include <cassert>
#include <cmath>
#include <iostream>
#include <string>

using namespace trx;

namespace {

void expect_close(double actual, double expected, double rel_tol = 1e-12) {
    const double diff = std::fabs(actual - expected);
    const double scale = std::max(1.0, std::fabs(expected));
    assert(diff / scale <= rel_tol);
}

void test_exact_pattern_difficulty() {
    auto suffix = estimate_pattern(PatternType::SUFFIX_CUSTOM, "8888");
    expect_close(suffix.expected_attempts, std::pow(58.0, 4.0));
    assert(suffix.difficulty != DifficultyRating::SIMPLE);

    auto prefix = estimate_pattern(PatternType::PREFIX_CUSTOM, "ABC");
    expect_close(prefix.expected_attempts, std::pow(58.0, 3.0));

    auto consecutive = estimate_pattern(PatternType::SUFFIX_CONSECUTIVE, "8", 7);
    expect_close(consecutive.expected_attempts, std::pow(58.0, 7.0));
    assert(consecutive.has_strong_warning);

    auto sequential = estimate_pattern(PatternType::SUFFIX_SEQUENTIAL, "1", 8);
    expect_close(sequential.expected_attempts, std::pow(58.0, 8.0));
    assert(sequential.has_strong_warning);
}

void test_contains_uses_multiple_windows() {
    auto suffix = estimate_pattern(PatternType::SUFFIX_CUSTOM, "5Z2");
    auto contains = estimate_pattern(PatternType::CONTAINS, "5Z2");
    const double exact_probability = std::pow(58.0, -3.0);
    const double expected_probability = 1.0 - std::pow(1.0 - exact_probability,
                                                       static_cast<double>(TRX_ADDRESS_BASE58_SIZE - 3));
    assert(contains.expected_attempts < suffix.expected_attempts);
    assert(contains.hit_probability_per_attempt > suffix.hit_probability_per_attempt);
    expect_close(contains.hit_probability_per_attempt, expected_probability);
}

void test_contains_accounts_for_fixed_tron_leading_t() {
    auto contains_t = estimate_pattern(PatternType::CONTAINS, "T");
    expect_close(contains_t.hit_probability_per_attempt, 1.0);
    expect_close(contains_t.expected_attempts, 1.0);

    auto runtime_t = estimate_runtime(contains_t, 1000.0, 1);
    expect_close(runtime_t.probability_progress, 1.0);

    auto contains_t8 = estimate_pattern(PatternType::CONTAINS, "T8");
    const double exact_t8 = std::pow(58.0, -2.0);
    const double leading_t8 = 1.0 / 58.0;
    const double expected_t8 = 1.0 - (1.0 - leading_t8) *
                                     std::pow(1.0 - exact_t8,
                                              static_cast<double>(TRX_ADDRESS_BASE58_SIZE - 2));
    expect_close(contains_t8.hit_probability_per_attempt, expected_t8);
    assert(contains_t8.hit_probability_per_attempt > leading_t8);
    assert(contains_t8.expected_attempts < 58.0);

    auto contains_t88 = estimate_pattern(PatternType::CONTAINS, "T88");
    const double exact_t88 = std::pow(58.0, -3.0);
    const double leading_t88 = std::pow(58.0, -2.0);
    const double expected_t88 = 1.0 - (1.0 - leading_t88) *
                                      std::pow(1.0 - exact_t88,
                                               static_cast<double>(TRX_ADDRESS_BASE58_SIZE - 3));
    expect_close(contains_t88.hit_probability_per_attempt, expected_t88);
    assert(contains_t88.expected_attempts < std::pow(58.0, 2.0));
}

void test_runtime_projection_and_probability_progress() {
    auto estimate = estimate_pattern(PatternType::SUFFIX_CUSTOM, "8888");
    auto runtime = estimate_runtime(estimate, 1000.0, 1000);
    assert(runtime.average_seconds > 0.0);
    const double expected_progress = 1.0 - std::pow(1.0 - estimate.hit_probability_per_attempt, 1000.0);
    expect_close(runtime.probability_progress, expected_progress, 1e-9);

    auto expected_attempt_runtime = estimate_runtime(
        estimate,
        1000.0,
        static_cast<uint64_t>(std::llround(estimate.expected_attempts)));
    assert(expected_attempt_runtime.probability_progress > 0.63);
    assert(expected_attempt_runtime.probability_progress < 0.64);

    std::string text = format_estimate_summary(estimate, runtime);
    assert(text.find("probability") != std::string::npos);
    assert(text.find("not guaranteed") != std::string::npos);
    assert(text.find("guaranteed") == std::string::npos || text.find("not guaranteed") != std::string::npos);
    const std::string banned_must_hit = std::string("\345\277\205") + "\345\207\272";
    assert(text.find(banned_must_hit) == std::string::npos);
}

void test_invalid_and_boundary_inputs() {
    assert(!is_estimable_pattern(PatternType::SUFFIX_CUSTOM, ""));
    assert(!is_estimable_pattern(PatternType::PREFIX_CUSTOM, "0"));
    assert(!is_estimable_pattern(PatternType::CONTAINS, std::string(21, 'A')));
    assert(is_estimable_pattern(PatternType::PREFIX_CUSTOM, "A"));
    assert(is_estimable_pattern(PatternType::SUFFIX_CONSECUTIVE, "8", 20));
    assert(!is_estimable_pattern(PatternType::SUFFIX_CONSECUTIVE, "8", 21));
}

} // namespace

int main() {
    test_exact_pattern_difficulty();
    test_contains_uses_multiple_windows();
    test_contains_accounts_for_fixed_tron_leading_t();
    test_runtime_projection_and_probability_progress();
    test_invalid_and_boundary_inputs();
    std::cout << "estimate tests passed\n";
    return 0;
}

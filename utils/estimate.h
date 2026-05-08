#ifndef TRX_ESTIMATE_H
#define TRX_ESTIMATE_H

#include "constants.h"
#include <cstdint>
#include <string>

namespace trx {

enum class DifficultyRating {
    SIMPLE,
    MODERATE,
    HARD,
    VERY_HARD,
    EXTREME
};

struct PatternEstimate {
    PatternType type;
    std::string pattern_text;
    size_t effective_length;
    double hit_probability_per_attempt;
    double expected_attempts;
    DifficultyRating difficulty;
    bool has_strong_warning;
};

struct RuntimeEstimate {
    double addr_per_second;
    double average_seconds;
    double probability_progress;
};

bool is_estimable_pattern(PatternType type,
                          const std::string& pattern_text,
                          size_t explicit_length = 0);

PatternEstimate estimate_pattern(PatternType type,
                                 const std::string& pattern_text,
                                 size_t explicit_length = 0);

RuntimeEstimate estimate_runtime(const PatternEstimate& estimate,
                                 double addr_per_second,
                                 uint64_t attempts_done = 0);

std::string difficulty_rating_label(DifficultyRating rating);
std::string format_duration(double seconds);
std::string format_estimate_summary(const PatternEstimate& estimate,
                                    const RuntimeEstimate& runtime = RuntimeEstimate{0.0, 0.0, 0.0});

} // namespace trx

#endif // TRX_ESTIMATE_H

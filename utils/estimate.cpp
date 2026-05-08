#include "estimate.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace trx {
namespace {

bool is_base58_text(const std::string& value) {
    return !value.empty() && std::all_of(value.begin(), value.end(), [](char c) {
        return std::string(BASE58_ALPHABET).find(c) != std::string::npos;
    });
}

size_t effective_length_for(PatternType type, const std::string& text, size_t explicit_length) {
    switch (type) {
        case PatternType::SUFFIX_CONSECUTIVE:
        case PatternType::SUFFIX_SEQUENTIAL:
            return explicit_length == 0 ? 7 : explicit_length;
        case PatternType::SUFFIX_CUSTOM:
        case PatternType::PREFIX_CUSTOM:
        case PatternType::CONTAINS:
            return text.size();
    }
    return text.size();
}

DifficultyRating rating_for_expected_attempts(double attempts) {
    if (attempts < 1.0e5) return DifficultyRating::SIMPLE;
    if (attempts < 1.0e7) return DifficultyRating::MODERATE;
    if (attempts < 1.0e10) return DifficultyRating::HARD;
    if (attempts < 1.0e13) return DifficultyRating::VERY_HARD;
    return DifficultyRating::EXTREME;
}

std::string format_number(double value) {
    std::ostringstream oss;
    if (value >= 1.0e9) {
        oss << std::scientific << std::setprecision(2) << value;
    } else if (value >= 1000.0) {
        oss << std::fixed << std::setprecision(0) << value;
    } else {
        oss << std::fixed << std::setprecision(2) << value;
    }
    return oss.str();
}

} // namespace

bool is_estimable_pattern(PatternType type, const std::string& pattern_text, size_t explicit_length) {
    const size_t length = effective_length_for(type, pattern_text, explicit_length);
    if (length == 0) return false;

    switch (type) {
        case PatternType::PREFIX_CUSTOM:
            return pattern_text.size() <= 10 && is_base58_text(pattern_text);
        case PatternType::SUFFIX_CUSTOM:
        case PatternType::CONTAINS:
            return pattern_text.size() <= 20 && is_base58_text(pattern_text);
        case PatternType::SUFFIX_CONSECUTIVE:
            return pattern_text.size() == 1 && pattern_text[0] >= '1' && pattern_text[0] <= '9' && length <= 20;
        case PatternType::SUFFIX_SEQUENTIAL:
            return pattern_text.size() == 1 && pattern_text[0] >= '0' && pattern_text[0] <= '9' && length <= 10 &&
                   (pattern_text[0] - '0') + static_cast<int>(length) - 1 <= 9;
    }
    return false;
}

PatternEstimate estimate_pattern(PatternType type, const std::string& pattern_text, size_t explicit_length) {
    if (!is_estimable_pattern(type, pattern_text, explicit_length)) {
        throw std::invalid_argument("pattern cannot be estimated");
    }

    const size_t length = effective_length_for(type, pattern_text, explicit_length);
    const double exact_probability = std::pow(static_cast<double>(BASE58_ALPHABET_SIZE), -static_cast<double>(length));
    double probability = exact_probability;

    if (type == PatternType::CONTAINS) {
        const size_t address_search_space = TRX_ADDRESS_BASE58_SIZE;
        const size_t other_windows = length > address_search_space ? 0 : (address_search_space - length);
        const double leading_window_probability =
            pattern_text[0] == 'T' ? std::pow(static_cast<double>(BASE58_ALPHABET_SIZE),
                                              -static_cast<double>(length - 1))
                                   : 0.0;
        probability = 1.0 - (1.0 - leading_window_probability) *
                            std::pow(1.0 - exact_probability, static_cast<double>(other_windows));
    }

    if (probability <= 0.0) {
        probability = std::numeric_limits<double>::min();
    }

    const double expected = 1.0 / probability;
    return PatternEstimate{type,
                           pattern_text,
                           length,
                           probability,
                           expected,
                           rating_for_expected_attempts(expected),
                           length >= 7};
}

RuntimeEstimate estimate_runtime(const PatternEstimate& estimate,
                                 double addr_per_second,
                                 uint64_t attempts_done) {
    const double safe_rate = addr_per_second > 0.0 ? addr_per_second : 0.0;
    const double average_seconds = safe_rate > 0.0 ? estimate.expected_attempts / safe_rate : 0.0;
    double progress = 0.0;
    if (attempts_done > 0 && estimate.hit_probability_per_attempt > 0.0) {
        if (estimate.hit_probability_per_attempt >= 1.0) {
            progress = 1.0;
        } else {
            progress = -std::expm1(static_cast<double>(attempts_done) *
                                   std::log1p(-estimate.hit_probability_per_attempt));
        }
    }
    if (progress > 1.0) progress = 1.0;
    return RuntimeEstimate{safe_rate, average_seconds, progress};
}

std::string difficulty_rating_label(DifficultyRating rating) {
    switch (rating) {
        case DifficultyRating::SIMPLE: return "simple";
        case DifficultyRating::MODERATE: return "moderate";
        case DifficultyRating::HARD: return "hard";
        case DifficultyRating::VERY_HARD: return "very hard";
        case DifficultyRating::EXTREME: return "extreme";
    }
    return "unknown";
}

std::string format_duration(double seconds) {
    if (seconds <= 0.0) return "unknown until speed is measured";
    std::ostringstream oss;
    if (seconds < 60.0) {
        oss << std::fixed << std::setprecision(1) << seconds << "s";
    } else if (seconds < 3600.0) {
        oss << std::fixed << std::setprecision(1) << seconds / 60.0 << "m";
    } else if (seconds < 86400.0) {
        oss << std::fixed << std::setprecision(1) << seconds / 3600.0 << "h";
    } else if (seconds < 31557600.0) {
        oss << std::fixed << std::setprecision(1) << seconds / 86400.0 << "d";
    } else {
        oss << std::fixed << std::setprecision(1) << seconds / 31557600.0 << "y";
    }
    return oss.str();
}

std::string format_estimate_summary(const PatternEstimate& estimate, const RuntimeEstimate& runtime) {
    std::ostringstream oss;
    oss << "Estimate: difficulty=" << difficulty_rating_label(estimate.difficulty)
        << ", avg attempts≈" << format_number(estimate.expected_attempts)
        << ", per-attempt probability≈" << std::scientific << std::setprecision(3)
        << estimate.hit_probability_per_attempt;
    if (runtime.addr_per_second > 0.0) {
        oss << std::defaultfloat << ", avg wait≈" << format_duration(runtime.average_seconds)
            << ", probability progress≈" << std::fixed << std::setprecision(4)
            << (runtime.probability_progress * 100.0) << "%";
    }
    oss << ". Probability is an average estimate and is not guaranteed.";
    if (estimate.has_strong_warning) {
        oss << " Warning: 7+ character vanity patterns can be extremely slow.";
    }
    return oss.str();
}

} // namespace trx

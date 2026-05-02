#ifndef TRX_PATTERN_H
#define TRX_PATTERN_H

#include "constants.h"
#include <string>
#include <vector>
#include <memory>
#include <functional>

namespace trx {

// Forward declarations
class PatternMatcher;

// ============================================================================
// Pattern Interface
// ============================================================================

class Pattern {
public:
    virtual ~Pattern() = default;

    // Check if a Base58 address string matches this pattern
    virtual bool matches(const std::string& address) const = 0;

    // Check if raw address bytes match (GPU-friendly binary check)
    // Returns true if potentially matches (may have false positives)
    virtual bool matches_bytes(const uint8_t* addr_bytes, size_t len) const = 0;

    // Get pattern description
    virtual std::string description() const = 0;

    // Get pattern type
    virtual PatternType type() const = 0;
};

// ============================================================================
// Suffix Consecutive Pattern (e.g., 8888888)
// ============================================================================

class SuffixConsecutivePattern : public Pattern {
public:
    // digit: '0'-'9', length: how many consecutive (e.g., 7 for 8888888)
    SuffixConsecutivePattern(char digit, size_t length);

    bool matches(const std::string& address) const override;
    bool matches_bytes(const uint8_t* addr_bytes, size_t len) const override;
    std::string description() const override;
    PatternType type() const override { return PatternType::SUFFIX_CONSECUTIVE; }

private:
    char digit_;
    size_t length_;
    std::string target_suffix_;

    // Precompute: which bytes/bits affect the trailing Base58 characters
    // For efficient GPU matching
    struct ByteMask {
        size_t byte_idx;
        uint8_t mask;
        uint8_t expected;
    };
    std::vector<ByteMask> byte_masks_;
    void compute_byte_masks();
};

// ============================================================================
// Suffix Sequential Pattern (e.g., 1234567, 7654321)
// ============================================================================

class SuffixSequentialPattern : public Pattern {
public:
    // start: starting digit, length: sequence length, ascending: true=123, false=321
    SuffixSequentialPattern(char start, size_t length, bool ascending);

    bool matches(const std::string& address) const override;
    bool matches_bytes(const uint8_t* addr_bytes, size_t len) const override;
    std::string description() const override;
    PatternType type() const override { return PatternType::SUFFIX_SEQUENTIAL; }

private:
    char start_;
    size_t length_;
    bool ascending_;
    std::string target_suffix_;
};

// ============================================================================
// Custom Suffix Pattern (e.g., "520", "1314")
// ============================================================================

class SuffixCustomPattern : public Pattern {
public:
    explicit SuffixCustomPattern(const std::string& suffix);

    bool matches(const std::string& address) const override;
    bool matches_bytes(const uint8_t* addr_bytes, size_t len) const override;
    std::string description() const override;
    PatternType type() const override { return PatternType::SUFFIX_CUSTOM; }

private:
    std::string suffix_;
};

// ============================================================================
// Prefix Custom Pattern (after 'T')
// ============================================================================

class PrefixCustomPattern : public Pattern {
public:
    explicit PrefixCustomPattern(const std::string& prefix);

    bool matches(const std::string& address) const override;
    bool matches_bytes(const uint8_t* addr_bytes, size_t len) const override;
    std::string description() const override;
    PatternType type() const override { return PatternType::PREFIX_CUSTOM; }

private:
    std::string prefix_;
};

// ============================================================================
// Contains Pattern
// ============================================================================

class ContainsPattern : public Pattern {
public:
    explicit ContainsPattern(const std::string& substring);

    bool matches(const std::string& address) const override;
    bool matches_bytes(const uint8_t* addr_bytes, size_t len) const override;
    std::string description() const override;
    PatternType type() const override { return PatternType::CONTAINS; }

private:
    std::string substring_;
};

// ============================================================================
// Pattern Factory
// ============================================================================

class PatternFactory {
public:
    static std::unique_ptr<Pattern> create(
        PatternType type,
        const std::string& param1,
        const std::string& param2 = ""
    );

    // Parse from CLI string
    // Format: "type:param" or "type:param1:param2"
    static std::unique_ptr<Pattern> parse(const std::string& spec);
};

// ============================================================================
// Multi-Pattern Matcher (OR logic)
// ============================================================================

class MultiPatternMatcher {
public:
    void add_pattern(std::unique_ptr<Pattern> pattern);
    bool matches_any(const std::string& address) const;
    std::vector<std::string> get_matching_patterns(const std::string& address) const;

private:
    std::vector<std::unique_ptr<Pattern>> patterns_;
};

} // namespace trx

#endif // TRX_PATTERN_H

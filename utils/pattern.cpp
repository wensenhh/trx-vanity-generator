#include "pattern.h"
#include <algorithm>
#include <sstream>

namespace trx {

// ============================================================================
// Suffix Consecutive Pattern
// ============================================================================

SuffixConsecutivePattern::SuffixConsecutivePattern(char digit, size_t length)
    : digit_(digit), length_(length) {
    if (digit < '0' || digit > '9') {
        throw std::invalid_argument("Digit must be 0-9");
    }
    if (length == 0 || length > 20) {
        throw std::invalid_argument("Length must be 1-20");
    }
    target_suffix_ = std::string(length, digit);
    compute_byte_masks();
}

void SuffixConsecutivePattern::compute_byte_masks() {
    // For a 7-digit consecutive suffix like "8888888":
    // We need to understand how Base58 encoding maps bits to characters
    // A TRX address is 25 bytes (with checksum) -> ~34 Base58 chars
    // The last N characters depend on the last ~N*log2(58)/8 bytes
    // For simplicity, we'll use a conservative byte mask approach

    // Approximate: each Base58 char ≈ log2(58) ≈ 5.86 bits
    // For N chars, we need about N * 5.86 / 8 bytes from the end
    size_t bytes_needed = static_cast<size_t>(length_ * 5.86 / 8.0) + 2;

    // This is a simplified approach - full implementation would precompute
    // exact bit boundaries for each character position
    for (size_t i = 0; i < std::min(bytes_needed, size_t{21}); ++i) {
        ByteMask mask;
        mask.byte_idx = 20 - i; // From end of address
        mask.mask = 0xFF;
        mask.expected = 0; // Will be set based on target
        byte_masks_.push_back(mask);
    }
}

bool SuffixConsecutivePattern::matches(const std::string& address) const {
    if (address.length() < length_) return false;
    return address.compare(address.length() - length_, length_, target_suffix_) == 0;
}

bool SuffixConsecutivePattern::matches_bytes(const uint8_t* addr_bytes, size_t len) const {
    // Conservative binary check - may have false positives
    // Full accuracy requires Base58 encode, done on CPU for candidates
    if (len < TRX_ADDRESS_SIZE) return false;

    // Quick heuristic: check if bytes suggest the pattern
    // This is intentionally permissive to avoid missing matches
    return true; // Let CPU do full verification
}

std::string SuffixConsecutivePattern::description() const {
    std::ostringstream oss;
    oss << length_ << " consecutive " << digit_ << "s at end";
    return oss.str();
}

// ============================================================================
// Suffix Sequential Pattern
// ============================================================================

SuffixSequentialPattern::SuffixSequentialPattern(char start, size_t length, bool ascending)
    : start_(start), length_(length), ascending_(ascending) {
    if (start < '0' || start > '9') {
        throw std::invalid_argument("Start digit must be 0-9");
    }
    if (length == 0 || length > 10) {
        throw std::invalid_argument("Length must be 1-10");
    }

    target_suffix_.reserve(length);
    char current = start;
    for (size_t i = 0; i < length; ++i) {
        target_suffix_ += current;
        if (ascending) {
            ++current;
        } else {
            --current;
        }
    }
}

bool SuffixSequentialPattern::matches(const std::string& address) const {
    if (address.length() < length_) return false;
    return address.compare(address.length() - length_, length_, target_suffix_) == 0;
}

bool SuffixSequentialPattern::matches_bytes(const uint8_t* addr_bytes, size_t len) const {
    if (len < TRX_ADDRESS_SIZE) return false;
    return true; // Conservative - CPU verifies
}

std::string SuffixSequentialPattern::description() const {
    std::ostringstream oss;
    oss << length_ << "-digit sequential " << (ascending_ ? "ascending" : "descending")
        << " starting from " << start_;
    return oss.str();
}

// ============================================================================
// Custom Suffix Pattern
// ============================================================================

SuffixCustomPattern::SuffixCustomPattern(const std::string& suffix)
    : suffix_(suffix) {
    if (suffix_.empty() || suffix_.length() > 20) {
        throw std::invalid_argument("Suffix length must be 1-20");
    }
    // Validate Base58 characters
    for (char c : suffix_) {
        if (!strchr(BASE58_ALPHABET, c)) {
            throw std::invalid_argument("Invalid Base58 character in suffix");
        }
    }
}

bool SuffixCustomPattern::matches(const std::string& address) const {
    if (address.length() < suffix_.length()) return false;
    return address.compare(address.length() - suffix_.length(), suffix_.length(), suffix_) == 0;
}

bool SuffixCustomPattern::matches_bytes(const uint8_t* addr_bytes, size_t len) const {
    if (len < TRX_ADDRESS_SIZE) return false;
    return true;
}

std::string SuffixCustomPattern::description() const {
    return "Suffix: " + suffix_;
}

// ============================================================================
// Prefix Custom Pattern
// ============================================================================

PrefixCustomPattern::PrefixCustomPattern(const std::string& prefix)
    : prefix_(prefix) {
    if (prefix_.empty() || prefix_.length() > 10) {
        throw std::invalid_argument("Prefix length must be 1-10");
    }
    for (char c : prefix_) {
        if (!strchr(BASE58_ALPHABET, c)) {
            throw std::invalid_argument("Invalid Base58 character in prefix");
        }
    }
}

bool PrefixCustomPattern::matches(const std::string& address) const {
    // Skip 'T' prefix
    if (address.length() < prefix_.length() + 1) return false;
    return address.compare(1, prefix_.length(), prefix_) == 0;
}

bool PrefixCustomPattern::matches_bytes(const uint8_t* addr_bytes, size_t len) const {
    if (len < TRX_ADDRESS_SIZE) return false;
    return true;
}

std::string PrefixCustomPattern::description() const {
    return "Prefix (after T): " + prefix_;
}

// ============================================================================
// Contains Pattern
// ============================================================================

ContainsPattern::ContainsPattern(const std::string& substring)
    : substring_(substring) {
    if (substring_.empty() || substring_.length() > 20) {
        throw std::invalid_argument("Substring length must be 1-20");
    }
    for (char c : substring_) {
        if (!strchr(BASE58_ALPHABET, c)) {
            throw std::invalid_argument("Invalid Base58 character");
        }
    }
}

bool ContainsPattern::matches(const std::string& address) const {
    return address.find(substring_) != std::string::npos;
}

bool ContainsPattern::matches_bytes(const uint8_t* addr_bytes, size_t len) const {
    if (len < TRX_ADDRESS_SIZE) return false;
    return true;
}

std::string ContainsPattern::description() const {
    return "Contains: " + substring_;
}

// ============================================================================
// Pattern Factory
// ============================================================================

std::unique_ptr<Pattern> PatternFactory::create(
    PatternType type,
    const std::string& param1,
    const std::string& param2) {

    switch (type) {
        case PatternType::SUFFIX_CONSECUTIVE: {
            if (param1.empty()) throw std::invalid_argument("Need digit parameter");
            char digit = param1[0];
            size_t length = param2.empty() ? 7 : std::stoul(param2);
            return std::make_unique<SuffixConsecutivePattern>(digit, length);
        }
        case PatternType::SUFFIX_SEQUENTIAL: {
            if (param1.empty()) throw std::invalid_argument("Need start digit");
            char start = param1[0];
            size_t length = param2.empty() ? 7 : std::stoul(param2);
            bool ascending = (param1.find("desc") == std::string::npos);
            return std::make_unique<SuffixSequentialPattern>(start, length, ascending);
        }
        case PatternType::SUFFIX_CUSTOM:
            return std::make_unique<SuffixCustomPattern>(param1);
        case PatternType::PREFIX_CUSTOM:
            return std::make_unique<PrefixCustomPattern>(param1);
        case PatternType::CONTAINS:
            return std::make_unique<ContainsPattern>(param1);
        default:
            throw std::invalid_argument("Unknown pattern type");
    }
}

std::unique_ptr<Pattern> PatternFactory::parse(const std::string& spec) {
    // Parse format: "type:param" or "type:param1:param2"
    size_t first_colon = spec.find(':');
    if (first_colon == std::string::npos) {
        throw std::invalid_argument("Pattern spec must be 'type:param'");
    }

    std::string type_str = spec.substr(0, first_colon);
    std::string rest = spec.substr(first_colon + 1);

    size_t second_colon = rest.find(':');
    std::string param1 = (second_colon == std::string::npos) ? rest : rest.substr(0, second_colon);
    std::string param2 = (second_colon == std::string::npos) ? "" : rest.substr(second_colon + 1);

    PatternType type;
    if (type_str == "consecutive" || type_str == "c") {
        type = PatternType::SUFFIX_CONSECUTIVE;
    } else if (type_str == "sequential" || type_str == "s") {
        type = PatternType::SUFFIX_SEQUENTIAL;
    } else if (type_str == "suffix" || type_str == "end") {
        type = PatternType::SUFFIX_CUSTOM;
    } else if (type_str == "prefix" || type_str == "start") {
        type = PatternType::PREFIX_CUSTOM;
    } else if (type_str == "contains" || type_str == "has") {
        type = PatternType::CONTAINS;
    } else {
        throw std::invalid_argument("Unknown pattern type: " + type_str);
    }

    return create(type, param1, param2);
}

// ============================================================================
// Multi-Pattern Matcher
// ============================================================================

void MultiPatternMatcher::add_pattern(std::unique_ptr<Pattern> pattern) {
    patterns_.push_back(std::move(pattern));
}

bool MultiPatternMatcher::matches_any(const std::string& address) const {
    for (const auto& pattern : patterns_) {
        if (pattern->matches(address)) {
            return true;
        }
    }
    return false;
}

std::vector<std::string> MultiPatternMatcher::get_matching_patterns(const std::string& address) const {
    std::vector<std::string> matches;
    for (const auto& pattern : patterns_) {
        if (pattern->matches(address)) {
            matches.push_back(pattern->description());
        }
    }
    return matches;
}

} // namespace trx

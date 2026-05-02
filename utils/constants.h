#ifndef TRX_CONSTANTS_H
#define TRX_CONSTANTS_H

#include <cstdint>
#include <array>
#include <vector>
#include <string>

namespace trx {

// TRON address constants
constexpr uint8_t TRX_ADDRESS_PREFIX = 0x41;
constexpr size_t TRX_ADDRESS_SIZE = 21;      // 1 prefix + 20 bytes
constexpr size_t TRX_ADDRESS_BASE58_SIZE = 34; // Base58 encoded length
constexpr size_t PRIVATE_KEY_SIZE = 32;
constexpr size_t PUBLIC_KEY_SIZE = 64;
constexpr size_t KECCAK256_SIZE = 32;

// Base58 alphabet (TRON uses same as Bitcoin)
constexpr char BASE58_ALPHABET[] = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";
constexpr size_t BASE58_ALPHABET_SIZE = 58;

// OpenCL work sizing
constexpr size_t DEFAULT_BATCH_SIZE = 65536;   // 64K addresses per batch
constexpr size_t DEFAULT_WORK_GROUP_SIZE = 256;

// Result buffer sizing
constexpr size_t MAX_RESULTS_PER_BATCH = 1024;

// Pattern types
enum class PatternType {
    SUFFIX_CONSECUTIVE,    // e.g., 8888888
    SUFFIX_SEQUENTIAL,     // e.g., 1234567 or 7654321
    SUFFIX_CUSTOM,         // user-defined suffix
    PREFIX_CUSTOM,         // user-defined prefix after 'T'
    CONTAINS               // contains substring
};

// GPU result structure (packed for OpenCL)
struct alignas(16) GPUMatchResult {
    uint32_t seed[4];      // 128-bit seed to regenerate private key
    uint32_t match_type;   // which pattern matched
    uint32_t reserved[3];  // padding
};

// CPU result structure
struct MatchResult {
    std::string address;       // Base58 TRX address
    std::string private_key_hex; // 64 hex chars
    std::string pattern_matched;
    uint64_t attempts;
};

} // namespace trx

#endif // TRX_CONSTANTS_H

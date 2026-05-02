#include "crypto.h"
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <cstring>

namespace trx {

// ============================================================================
// Keccak-256 Implementation (FIPS 202)
// ============================================================================
// Keccak uses different padding than SHA3: "01" suffix instead of "06"

struct Keccak256::Impl {
    static constexpr size_t RATE = 136;  // 1088 bits = 136 bytes for 256-bit output
    static constexpr size_t CAPACITY = 64; // 512 bits
    static constexpr size_t STATE_SIZE = 25; // 1600 bits / 64
    static constexpr uint64_t RC[24] = {
        0x0000000000000001ULL, 0x0000000000008082ULL, 0x800000000000808aULL,
        0x8000000080008000ULL, 0x000000000000808bULL, 0x0000000080000001ULL,
        0x8000000080008081ULL, 0x8000000000008009ULL, 0x000000000000008aULL,
        0x0000000000000088ULL, 0x0000000080008009ULL, 0x000000008000000aULL,
        0x000000008000808bULL, 0x800000000000008bULL, 0x8000000000008089ULL,
        0x8000000000008003ULL, 0x8000000000008002ULL, 0x8000000000000080ULL,
        0x000000000000800aULL, 0x800000008000000aULL, 0x8000000080008081ULL,
        0x8000000000008080ULL, 0x0000000080000001ULL, 0x8000000080008008ULL
    };

    uint64_t state[STATE_SIZE];
    uint8_t buffer[RATE];
    size_t buffer_size;
    bool finalized;

    Impl() : buffer_size(0), finalized(false) {
        reset();
    }

    void reset() {
        memset(state, 0, sizeof(state));
        memset(buffer, 0, sizeof(buffer));
        buffer_size = 0;
        finalized = false;
    }

    static uint64_t rol64(uint64_t x, int n) {
        return (x << n) | (x >> (64 - n));
    }

    void keccak_f() {
        uint64_t t[5];
        uint64_t bc[5];

        for (int round = 0; round < 24; ++round) {
            // Theta
            for (int i = 0; i < 5; ++i) {
                bc[i] = state[i] ^ state[i + 5] ^ state[i + 10] ^ state[i + 15] ^ state[i + 20];
            }
            for (int i = 0; i < 5; ++i) {
                t[i] = bc[(i + 4) % 5] ^ rol64(bc[(i + 1) % 5], 1);
            }
            for (int i = 0; i < 25; ++i) {
                state[i] ^= t[i % 5];
            }

            // Rho and Pi
            uint64_t last = state[1];
            uint64_t temp;
            const int piln[24] = {
                10, 7, 11, 17, 18, 3, 5, 16, 8, 21, 24, 4,
                15, 23, 19, 13, 12, 2, 20, 14, 22, 9, 6, 1
            };
            const int rotc[24] = {
                1, 3, 6, 10, 15, 21, 28, 36, 45, 55, 2, 14,
                27, 41, 56, 8, 25, 43, 62, 18, 39, 61, 20, 44
            };
            for (int i = 0; i < 24; ++i) {
                temp = state[piln[i]];
                state[piln[i]] = rol64(last, rotc[i]);
                last = temp;
            }

            // Chi
            for (int j = 0; j < 25; j += 5) {
                for (int i = 0; i < 5; ++i) {
                    t[i] = state[j + i];
                }
                for (int i = 0; i < 5; ++i) {
                    state[j + i] ^= (~t[(i + 1) % 5]) & t[(i + 2) % 5];
                }
            }

            // Iota
            state[0] ^= RC[round];
        }
    }

    void absorb(const uint8_t* data, size_t len) {
        size_t i = 0;
        while (i < len) {
            size_t chunk = std::min(len - i, RATE - buffer_size);
            memcpy(buffer + buffer_size, data + i, chunk);
            buffer_size += chunk;
            i += chunk;

            if (buffer_size == RATE) {
                for (size_t j = 0; j < RATE / 8; ++j) {
                    state[j] ^= ((uint64_t*)buffer)[j];
                }
                keccak_f();
                buffer_size = 0;
            }
        }
    }

    void pad() {
        // Keccak padding: append 0x01 then 0x00... with final 0x80
        buffer[buffer_size] = 0x01;
        for (size_t i = buffer_size + 1; i < RATE; ++i) {
            buffer[i] = 0;
        }
        buffer[RATE - 1] |= 0x80;

        for (size_t j = 0; j < RATE / 8; ++j) {
            state[j] ^= ((uint64_t*)buffer)[j];
        }
        keccak_f();
    }

    void squeeze(uint8_t* output, size_t len) {
        size_t offset = 0;
        while (offset < len) {
            size_t chunk = std::min(len - offset, RATE);
            memcpy(output + offset, state, chunk);
            offset += chunk;
            if (offset < len) {
                keccak_f();
            }
        }
    }
};

Keccak256::Keccak256() : pimpl(std::make_unique<Impl>()) {}
Keccak256::~Keccak256() = default;

void Keccak256::reset() {
    pimpl->reset();
}

void Keccak256::update(const uint8_t* data, size_t len) {
    pimpl->absorb(data, len);
}

std::array<uint8_t, KECCAK256_SIZE> Keccak256::finalize() {
    if (!pimpl->finalized) {
        pimpl->pad();
        pimpl->finalized = true;
    }
    std::array<uint8_t, KECCAK256_SIZE> result;
    pimpl->squeeze(result.data(), KECCAK256_SIZE);
    return result;
}

std::array<uint8_t, KECCAK256_SIZE> Keccak256::hash(const uint8_t* data, size_t len) {
    Keccak256 k;
    k.update(data, len);
    return k.finalize();
}

// ============================================================================
// secp256k1 ECC Implementation (using OpenSSL)
// ============================================================================

Secp256k1::Secp256k1() {
    ec_key_ = EC_KEY_new_by_curve_name(NID_secp256k1);
    ec_group_ = (EC_GROUP*)EC_KEY_get0_group(ec_key_);
    bn_ctx_ = BN_CTX_new();
}

Secp256k1::~Secp256k1() {
    EC_KEY_free(ec_key_);
    BN_CTX_free(bn_ctx_);
}

std::array<uint8_t, PUBLIC_KEY_SIZE> Secp256k1::generate_public_key(
    const std::array<uint8_t, PRIVATE_KEY_SIZE>& private_key) {

    BIGNUM* priv_bn = BN_bin2bn(private_key.data(), PRIVATE_KEY_SIZE, nullptr);
    if (!priv_bn) {
        throw std::runtime_error("Failed to convert private key to BIGNUM");
    }

    EC_POINT* pub_point = EC_POINT_new(ec_group_);
    if (!pub_point) {
        BN_free(priv_bn);
        throw std::runtime_error("Failed to create EC_POINT");
    }

    // pub = priv * G
    if (!EC_POINT_mul(ec_group_, pub_point, priv_bn, nullptr, nullptr, bn_ctx_)) {
        EC_POINT_free(pub_point);
        BN_free(priv_bn);
        throw std::runtime_error("EC_POINT_mul failed");
    }

    BN_free(priv_bn);

    // Get uncompressed public key: 0x04 + X(32) + Y(32)
    std::array<uint8_t, PUBLIC_KEY_SIZE + 1> pub_with_prefix;
    size_t pub_len = EC_POINT_point2oct(ec_group_, pub_point,
        POINT_CONVERSION_UNCOMPRESSED, pub_with_prefix.data(),
        pub_with_prefix.size(), bn_ctx_);

    EC_POINT_free(pub_point);

    if (pub_len != PUBLIC_KEY_SIZE + 1) {
        throw std::runtime_error("Unexpected public key length");
    }

    // Skip 0x04 prefix
    std::array<uint8_t, PUBLIC_KEY_SIZE> result;
    memcpy(result.data(), pub_with_prefix.data() + 1, PUBLIC_KEY_SIZE);
    return result;
}

std::array<uint8_t, TRX_ADDRESS_SIZE> Secp256k1::generate_address_bytes(
    const std::array<uint8_t, PUBLIC_KEY_SIZE>& public_key) {

    auto hash = Keccak256::hash(public_key.data(), PUBLIC_KEY_SIZE);

    std::array<uint8_t, TRX_ADDRESS_SIZE> address;
    address[0] = TRX_ADDRESS_PREFIX;
    memcpy(address.data() + 1, hash.data() + 12, 20); // Last 20 bytes

    return address;
}

// ============================================================================
// Base58 Encoding
// ============================================================================

std::string Base58::encode_raw(const uint8_t* data, size_t len) {
    if (len == 0) return "";

    // Count leading zeros
    size_t leading_zeros = 0;
    while (leading_zeros < len && data[leading_zeros] == 0) {
        ++leading_zeros;
    }

    // Convert to base58
    // log(256) / log(58) ≈ 1.38, so max output = len * 138 / 100 + 1
    size_t max_output = len * 138 / 100 + 1;
    std::vector<uint8_t> digits(max_output, 0);

    for (size_t i = leading_zeros; i < len; ++i) {
        uint32_t carry = data[i];
        for (size_t j = 0; j < digits.size(); ++j) {
            carry += (uint32_t)digits[j] * 256;
            digits[j] = carry % 58;
            carry /= 58;
        }
    }

    // Build result string
    std::string result;
    result.reserve(leading_zeros + digits.size());
    result.append(leading_zeros, '1'); // '1' is the Base58 representation of 0

    // Append digits in reverse order (skip leading zeros in digits)
    size_t digit_start = digits.size();
    while (digit_start > 0 && digits[digit_start - 1] == 0) {
        --digit_start;
    }
    for (size_t i = digit_start; i > 0; --i) {
        result += BASE58_ALPHABET[digits[i - 1]];
    }

    return result;
}

std::string Base58::encode_check(const uint8_t* data, size_t len) {
    // Append double SHA256 checksum (first 4 bytes)
    std::vector<uint8_t> with_checksum(len + 4);
    memcpy(with_checksum.data(), data, len);

    uint8_t hash1[SHA256_DIGEST_LENGTH];
    uint8_t hash2[SHA256_DIGEST_LENGTH];
    SHA256(data, len, hash1);
    SHA256(hash1, SHA256_DIGEST_LENGTH, hash2);
    memcpy(with_checksum.data() + len, hash2, 4);

    return encode_raw(with_checksum.data(), with_checksum.size());
}

std::string Base58::encode_address(const std::array<uint8_t, TRX_ADDRESS_SIZE>& addr) {
    return encode_check(addr.data(), TRX_ADDRESS_SIZE);
}

std::vector<uint8_t> Base58::decode(const std::string& str) {
    // Count leading '1's (which represent zeros)
    size_t leading_ones = 0;
    while (leading_ones < str.size() && str[leading_ones] == '1') {
        ++leading_ones;
    }

    // Convert from base58
    std::vector<uint8_t> bytes((str.size() - leading_ones) * 733 / 1000 + 1, 0);

    for (size_t i = leading_ones; i < str.size(); ++i) {
        const char* p = strchr(BASE58_ALPHABET, str[i]);
        if (!p) {
            throw std::runtime_error("Invalid Base58 character");
        }
        uint32_t carry = p - BASE58_ALPHABET;
        for (size_t j = 0; j < bytes.size(); ++j) {
            carry += (uint32_t)bytes[j] * 58;
            bytes[j] = carry % 256;
            carry /= 256;
        }
    }

    // Find first non-zero byte
    size_t start = bytes.size();
    while (start > 0 && bytes[start - 1] == 0) {
        --start;
    }

    // Build result with leading zeros
    std::vector<uint8_t> result(leading_ones + (bytes.size() - start));
    memset(result.data(), 0, leading_ones);
    for (size_t i = 0; i < bytes.size() - start; ++i) {
        result[leading_ones + i] = bytes[bytes.size() - 1 - i];
    }

    return result;
}

// ============================================================================
// Utility Functions
// ============================================================================

std::string bytes_to_hex(const uint8_t* data, size_t len) {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (size_t i = 0; i < len; ++i) {
        oss << std::setw(2) << (int)data[i];
    }
    return oss.str();
}

std::array<uint8_t, PRIVATE_KEY_SIZE> hex_to_private_key(const std::string& hex) {
    if (hex.length() != PRIVATE_KEY_SIZE * 2) {
        throw std::runtime_error("Invalid private key hex length");
    }

    std::array<uint8_t, PRIVATE_KEY_SIZE> result;
    for (size_t i = 0; i < PRIVATE_KEY_SIZE; ++i) {
        result[i] = static_cast<uint8_t>(std::stoi(hex.substr(i * 2, 2), nullptr, 16));
    }
    return result;
}

} // namespace trx

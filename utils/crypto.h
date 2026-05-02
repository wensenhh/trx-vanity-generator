#ifndef TRX_CRYPTO_H
#define TRX_CRYPTO_H

#include "constants.h"
#include <openssl/evp.h>
#include <openssl/bn.h>
#include <openssl/ec.h>
#include <openssl/sha.h>
#include <array>
#include <vector>
#include <string>

namespace trx {

// Keccak-256 implementation (FIPS 202, different from SHA3)
// TRON uses original Keccak, not NIST SHA3
class Keccak256 {
public:
    Keccak256();
    ~Keccak256();

    void reset();
    void update(const uint8_t* data, size_t len);
    std::array<uint8_t, KECCAK256_SIZE> finalize();

    // One-shot hash
    static std::array<uint8_t, KECCAK256_SIZE> hash(const uint8_t* data, size_t len);

private:
    struct Impl;
    std::unique_ptr<Impl> pimpl;
};

// secp256k1 ECC operations
class Secp256k1 {
public:
    Secp256k1();
    ~Secp256k1();

    // Generate public key from private key
    // Returns 64-byte uncompressed public key (X || Y)
    std::array<uint8_t, PUBLIC_KEY_SIZE> generate_public_key(
        const std::array<uint8_t, PRIVATE_KEY_SIZE>& private_key
    );

    // Generate TRX address bytes (21 bytes: 0x41 + hash[12:32])
    std::array<uint8_t, TRX_ADDRESS_SIZE> generate_address_bytes(
        const std::array<uint8_t, PUBLIC_KEY_SIZE>& public_key
    );

private:
    EC_KEY* ec_key_;
    EC_GROUP* ec_group_;
    BN_CTX* bn_ctx_;
};

// Base58Check encoding
class Base58 {
public:
    // Encode TRX address bytes to Base58 string
    static std::string encode_address(const std::array<uint8_t, TRX_ADDRESS_SIZE>& addr);

    // Encode with checksum (double SHA256)
    static std::string encode_check(const uint8_t* data, size_t len);

    // Decode Base58 to bytes
    static std::vector<uint8_t> decode(const std::string& str);

private:
    static std::string encode_raw(const uint8_t* data, size_t len);
};

// Utility functions
std::string bytes_to_hex(const uint8_t* data, size_t len);
std::array<uint8_t, PRIVATE_KEY_SIZE> hex_to_private_key(const std::string& hex);

} // namespace trx

#endif // TRX_CRYPTO_H

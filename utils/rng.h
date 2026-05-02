#ifndef TRX_RNG_H
#define TRX_RNG_H

#include "constants.h"
#include <random>
#include <array>
#include <cstdint>

namespace trx {

// ============================================================================
// CPU Random Number Generator
// Uses C++11 std::random_device + std::mt19937_64 for seeding
// ============================================================================

class RNG {
public:
    RNG();
    explicit RNG(uint64_t seed);

    // Generate 32 bytes for private key
    std::array<uint8_t, PRIVATE_KEY_SIZE> generate_private_key();

    // Generate 128-bit seed for GPU work-item
    std::array<uint32_t, 4> generate_seed();

    // Generate batch of seeds
    std::vector<std::array<uint32_t, 4>> generate_seeds(size_t count);

    // Convert seed to bytes (for deterministic private key derivation)
    std::array<uint8_t, PRIVATE_KEY_SIZE> seed_to_bytes(const std::array<uint32_t, 4>& seed);

    // Derive private key from seed bytes
    std::array<uint8_t, PRIVATE_KEY_SIZE> derive_private_key(const std::array<uint8_t, PRIVATE_KEY_SIZE>& bytes);

private:
    std::mt19937_64 engine_;
    std::uniform_int_distribution<uint64_t> dist_;
};

// ============================================================================
// xoshiro256++ - High-quality PRNG (for reference, GPU version in kernel)
// ============================================================================

class Xoshiro256PlusPlus {
public:
    explicit Xoshiro256PlusPlus(uint64_t seed);

    uint64_t next();
    void jump();
    void long_jump();

private:
    uint64_t state_[4];

    static uint64_t rotl(uint64_t x, int k) {
        return (x << k) | (x >> (64 - k));
    }
};

} // namespace trx

#endif // TRX_RNG_H

#include "rng.h"
#include <chrono>
#include <cstring>

namespace trx {

// ============================================================================
// CPU RNG Implementation
// ============================================================================

RNG::RNG() {
    std::random_device rd;
    uint64_t seed = rd();
    seed ^= static_cast<uint64_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count());
    engine_.seed(seed);
}

RNG::RNG(uint64_t seed) : engine_(seed) {}

std::array<uint8_t, PRIVATE_KEY_SIZE> RNG::generate_private_key() {
    std::array<uint8_t, PRIVATE_KEY_SIZE> key;
    for (size_t i = 0; i < PRIVATE_KEY_SIZE; i += 8) {
        uint64_t val = dist_(engine_);
        memcpy(key.data() + i, &val, std::min(size_t{8}, PRIVATE_KEY_SIZE - i));
    }
    return key;
}

std::array<uint32_t, 4> RNG::generate_seed() {
    std::array<uint32_t, 4> seed;
    for (int i = 0; i < 4; ++i) {
        seed[i] = static_cast<uint32_t>(dist_(engine_));
    }
    return seed;
}

std::vector<std::array<uint32_t, 4>> RNG::generate_seeds(size_t count) {
    std::vector<std::array<uint32_t, 4>> seeds;
    seeds.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        seeds.push_back(generate_seed());
    }
    return seeds;
}

// Deterministic seed-to-private-key conversion (must match GPU kernel logic)
std::array<uint8_t, PRIVATE_KEY_SIZE> RNG::seed_to_bytes(const std::array<uint32_t, 4>& seed) {
    std::array<uint8_t, PRIVATE_KEY_SIZE> bytes;
    // Use xoshiro256++ to expand 128-bit seed to 256-bit private key
    uint64_t s0 = ((uint64_t)seed[0] << 32) | seed[1];
    uint64_t s1 = ((uint64_t)seed[2] << 32) | seed[3];
    uint64_t s2 = s0 ^ s1;
    uint64_t s3 = s0 ^ (s1 >> 17);

    // SplitMix64 to initialize xoshiro state
    uint64_t state[4];
    for (int i = 0; i < 4; ++i) {
        s3 += 0x9e3779b97f4a7c15ULL;
        uint64_t z = s3;
        z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
        z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
        state[i] = z ^ (z >> 31);
    }

    // xoshiro256++ to generate 32 bytes
    auto rotl = [](uint64_t x, int k) { return (x << k) | (x >> (64 - k)); };
    for (int i = 0; i < 4; ++i) {
        uint64_t result = rotl(state[0] + state[3], 23) + state[0];
        uint64_t t = state[1] << 17;
        state[2] ^= state[0];
        state[3] ^= state[1];
        state[1] ^= state[2];
        state[0] ^= state[3];
        state[2] ^= t;
        state[3] = rotl(state[3], 45);

        bytes[i * 8 + 0] = (uint8_t)(result >> 56);
        bytes[i * 8 + 1] = (uint8_t)(result >> 48);
        bytes[i * 8 + 2] = (uint8_t)(result >> 40);
        bytes[i * 8 + 3] = (uint8_t)(result >> 32);
        bytes[i * 8 + 4] = (uint8_t)(result >> 24);
        bytes[i * 8 + 5] = (uint8_t)(result >> 16);
        bytes[i * 8 + 6] = (uint8_t)(result >> 8);
        bytes[i * 8 + 7] = (uint8_t)(result);
    }
    return bytes;
}

std::array<uint8_t, PRIVATE_KEY_SIZE> RNG::derive_private_key(const std::array<uint8_t, PRIVATE_KEY_SIZE>& bytes) {
    // For now, direct pass-through. In production, may apply clamping or other transforms.
    return bytes;
}

// ============================================================================
// xoshiro256++ Implementation
// ============================================================================

Xoshiro256PlusPlus::Xoshiro256PlusPlus(uint64_t seed) {
    // SplitMix64 to initialize state from single seed
    uint64_t z = seed + 0x9e3779b97f4a7c15ULL;
    for (int i = 0; i < 4; ++i) {
        z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
        z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
        state_[i] = z ^ (z >> 31);
    }
}

uint64_t Xoshiro256PlusPlus::next() {
    const uint64_t result = rotl(state_[0] + state_[3], 23) + state_[0];
    const uint64_t t = state_[1] << 17;

    state_[2] ^= state_[0];
    state_[3] ^= state_[1];
    state_[1] ^= state_[2];
    state_[0] ^= state_[3];

    state_[2] ^= t;
    state_[3] = rotl(state_[3], 45);

    return result;
}

void Xoshiro256PlusPlus::jump() {
    static const uint64_t JUMP[] = {
        0x180ec6d33cfd0abaULL, 0xd5a61266f0c9392cULL,
        0xa9582618e03fc9aaULL, 0x39abdc4529b1661cULL
    };

    uint64_t s0 = 0, s1 = 0, s2 = 0, s3 = 0;
    for (int i = 0; i < 4; ++i) {
        for (int b = 0; b < 64; ++b) {
            if (JUMP[i] & (1ULL << b)) {
                s0 ^= state_[0];
                s1 ^= state_[1];
                s2 ^= state_[2];
                s3 ^= state_[3];
            }
            next();
        }
    }
    state_[0] = s0;
    state_[1] = s1;
    state_[2] = s2;
    state_[3] = s3;
}

void Xoshiro256PlusPlus::long_jump() {
    static const uint64_t LONG_JUMP[] = {
        0x76e15d3efefdcbbfULL, 0xc5004e441c522fb3ULL,
        0x77710069854ee241ULL, 0x39109bb02acbe635ULL
    };

    uint64_t s0 = 0, s1 = 0, s2 = 0, s3 = 0;
    for (int i = 0; i < 4; ++i) {
        for (int b = 0; b < 64; ++b) {
            if (LONG_JUMP[i] & (1ULL << b)) {
                s0 ^= state_[0];
                s1 ^= state_[1];
                s2 ^= state_[2];
                s3 ^= state_[3];
            }
            next();
        }
    }
    state_[0] = s0;
    state_[1] = s1;
    state_[2] = s2;
    state_[3] = s3;
}

} // namespace trx

// CACHE_BUST: 744656
// ============================================================================
// TRON Vanity Address Generator - OpenCL Kernel
// Phase 2: GPU-accelerated Keccak256 + address matching
// Phase 3: Full GPU ECC (secp256k1)
// ============================================================================

// Base58 alphabet for pattern matching
constant char BASE58_CHARS[] = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";

// Host-compatible GPU result structure (matches GPUMatchResult in constants.h).
typedef struct {
    uint seed[4];
    uint match_type;
    uint reserved[3];
} gpu_match_result;


// ============================================================================
// Utility / RNG Functions
// ============================================================================

ulong rol64_rng(ulong x, int n) {
    return (x << n) | (x >> (64 - n));
}

uint4 xoshiro_next(ulong4* state) {
    ulong result = rol64_rng(state->x + state->w, 23) + state->x;
    ulong t = state->y << 17;

    state->z ^= state->x;
    state->w ^= state->y;
    state->y ^= state->z;
    state->x ^= state->w;
    state->z ^= t;
    state->w = rol64_rng(state->w, 45);

    return (uint4)(
        (uint)(result >> 32),
        (uint)(result & 0xFFFFFFFF),
        (uint)(result >> 32) ^ (uint)(result & 0xFFFFFFFF),
        (uint)(result >> 16) & 0xFFFFFFFF
    );
}

ulong xoshiro_next64(ulong4* state) {
    ulong result = rol64_rng(state->x + state->w, 23) + state->x;
    ulong t = state->y << 17;

    state->z ^= state->x;
    state->w ^= state->y;
    state->y ^= state->z;
    state->x ^= state->w;
    state->z ^= t;
    state->w = rol64_rng(state->w, 45);

    return result;
}

void init_rng(ulong4* state, uint4 seed, uint gid) {
    (void)gid;
    ulong s0 = ((ulong)seed.x << 32) | seed.y;
    ulong s1 = ((ulong)seed.z << 32) | seed.w;
    ulong s3 = s0 ^ (s1 >> 17);

    for (int i = 0; i < 4; i++) {
        s3 += 0x9e3779b97f4a7c15UL;
        ulong z = s3;
        z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9UL;
        z = (z ^ (z >> 27)) * 0x94d049bb133111ebUL;
        switch(i) {
            case 0: state->x = z ^ (z >> 31); break;
            case 1: state->y = z ^ (z >> 31); break;
            case 2: state->z = z ^ (z >> 31); break;
            case 3: state->w = z ^ (z >> 31); break;
        }
    }
}

// ============================================================================
// Pattern Matching (GPU-Optimized)
// ============================================================================

// Check if address bytes would produce a suffix with consecutive digits
// This is a probabilistic check - exact match done on CPU
bool check_consecutive_suffix(const uchar* addr_bytes, uint len, char digit, uint target_len) {
    // Simplified: check if last bytes suggest consecutive pattern
    // Full implementation needs Base58 character mapping
    // For now: conservative check that passes candidates to CPU
    return true;
}

// Check sequential pattern (ascending/descending)
bool check_sequential_suffix(const uchar* addr_bytes, uint len, char start, uint target_len, bool ascending) {
    // Conservative check
    return true;
}

// ============================================================================
// Main Kernel (Phase 2)
// ============================================================================

// Phase 2: GPU does RNG + Keccak256, CPU does ECC + Base58 + match
// Phase 3: GPU does everything except final Base58 encode

__kernel void generate_addresses(
    __global const uint4* seeds,
    __global gpu_match_result* results,
    __global uint* match_count,
    uint batch_size
) {
    uint gid = get_global_id(0);
    if (gid >= batch_size) return;

    ulong4 rng_state;
    init_rng(&rng_state, seeds[gid], gid);

    uchar private_key[32];
    for (int i = 0; i < 8; i++) {
        uint4 r = xoshiro_next(&rng_state);
        private_key[i * 4 + 0] = (uchar)(r.x >> 24);
        private_key[i * 4 + 1] = (uchar)(r.x >> 16);
        private_key[i * 4 + 2] = (uchar)(r.x >> 8);
        private_key[i * 4 + 3] = (uchar)(r.x);
    }

    uint idx = atomic_inc(match_count);
    if (idx < batch_size) {
        results[idx].seed[0] = seeds[gid].x;
        results[idx].seed[1] = seeds[gid].y;
        results[idx].seed[2] = seeds[gid].z;
        results[idx].seed[3] = seeds[gid].w;
        results[idx].match_type = 0;
        results[idx].reserved[0] = 0;
        results[idx].reserved[1] = 0;
        results[idx].reserved[2] = 0;
    }
}

// ============================================================================
// Phase 3: Full GPU Pipeline (ECC + Keccak256 + Address Matching)
// Uses the validated STANDARD-arithmetic ECC path from kernel/test_ecc.cl.
// ============================================================================

#include "test_ecc.cl"

__kernel void generate_addresses_full_gpu(
    __global const uint4* seeds,
    __global gpu_match_result* results,
    __global uint* match_count,
    __global uchar* addresses_out,
    uint batch_size
) {
    uint gid = get_global_id(0);
    if (gid >= batch_size) return;

    uint256 prime;
    prime.d[0] = 0xFFFFFFFEFFFFFC2FUL;
    prime.d[1] = 0xFFFFFFFFFFFFFFFFUL;
    prime.d[2] = 0xFFFFFFFFFFFFFFFFUL;
    prime.d[3] = 0xFFFFFFFFFFFFFFFFUL;

    // Initialize RNG (must match CPU seed_to_bytes exactly)
    ulong4 rng_state;
    init_rng(&rng_state, seeds[gid], gid);

    // Generate private key. Must match RNG::seed_to_bytes exactly: four
    // xoshiro256++ 64-bit outputs, big-endian bytes.
    uchar private_key[32];
    for (int i = 0; i < 4; i++) {
        ulong r = xoshiro_next64(&rng_state);
        private_key[i * 8 + 0] = (uchar)(r >> 56);
        private_key[i * 8 + 1] = (uchar)(r >> 48);
        private_key[i * 8 + 2] = (uchar)(r >> 40);
        private_key[i * 8 + 3] = (uchar)(r >> 32);
        private_key[i * 8 + 4] = (uchar)(r >> 24);
        private_key[i * 8 + 5] = (uchar)(r >> 16);
        private_key[i * 8 + 6] = (uchar)(r >> 8);
        private_key[i * 8 + 7] = (uchar)(r);
    }

    // ECC: private key -> public key
    uchar public_key[64];
    generate_public_key_std(public_key, private_key, &prime);

    // Keccak256(public_key)
    uchar hash[32];
    keccak256(hash, public_key, 64);

    // Build TRX address bytes: 0x41 + hash[12:32]
    uchar address_bytes[21];
    address_bytes[0] = 0x41;
    for (int i = 0; i < 20; i++) {
        address_bytes[i + 1] = hash[12 + i];
    }

    // ============================================================================
    // Pattern Matching: GPU pre-filter + CPU verification
    // ============================================================================
    // We do a lightweight GPU-side filter that catches most patterns,
    // then CPU does full Base58 verification.
    //
    // Pattern types supported on GPU:
    // 0 = suffix (check last N bytes of address)
    // 1 = prefix (check first N bytes after 0x41)
    // 2 = contains (anywhere in address)
    // 3 = consecutive suffix (repeated byte at end)
    // 4 = sequential suffix (ascending bytes at end)
    //
    // Pattern buffer layout (from host):
    // [0] = pattern_type
    // [1] = pattern_len
    // [2..21] = pattern bytes (up to 20 bytes)
    // For consecutive/sequential: [2] = target digit/byte
    // ============================================================================

    bool is_match = false;

    // Default: return ALL addresses for CPU verification
    // This ensures correctness while we implement proper GPU filtering
    // TODO: Phase 3b - implement proper GPU pattern matching
    is_match = true;

    if (is_match) {
        uint idx = atomic_inc(match_count);
        if (idx < batch_size) {
            results[idx].seed[0] = seeds[gid].x;
            results[idx].seed[1] = seeds[gid].y;
            results[idx].seed[2] = seeds[gid].z;
            results[idx].seed[3] = seeds[gid].w;
            results[idx].match_type = 0;
            results[idx].reserved[0] = 0;
            results[idx].reserved[1] = 0;
            results[idx].reserved[2] = 0;

            // Copy address bytes to output
            for (int i = 0; i < 21; i++) {
                addresses_out[idx * 21 + i] = address_bytes[i];
            }
        }
    }
}

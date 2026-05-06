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
// Exact Base58Check + Pattern Matching
// ============================================================================

uint rotr32(uint x, uint n) { return (x >> n) | (x << (32 - n)); }
uint ch32(uint x, uint y, uint z) { return (x & y) ^ (~x & z); }
uint maj32(uint x, uint y, uint z) { return (x & y) ^ (x & z) ^ (y & z); }
uint bsig0(uint x) { return rotr32(x, 2) ^ rotr32(x, 13) ^ rotr32(x, 22); }
uint bsig1(uint x) { return rotr32(x, 6) ^ rotr32(x, 11) ^ rotr32(x, 25); }
uint ssig0(uint x) { return rotr32(x, 7) ^ rotr32(x, 18) ^ (x >> 3); }
uint ssig1(uint x) { return rotr32(x, 17) ^ rotr32(x, 19) ^ (x >> 10); }

constant uint SHA256_K[64] = {
    0x428a2f98U,0x71374491U,0xb5c0fbcfU,0xe9b5dba5U,0x3956c25bU,0x59f111f1U,0x923f82a4U,0xab1c5ed5U,
    0xd807aa98U,0x12835b01U,0x243185beU,0x550c7dc3U,0x72be5d74U,0x80deb1feU,0x9bdc06a7U,0xc19bf174U,
    0xe49b69c1U,0xefbe4786U,0x0fc19dc6U,0x240ca1ccU,0x2de92c6fU,0x4a7484aaU,0x5cb0a9dcU,0x76f988daU,
    0x983e5152U,0xa831c66dU,0xb00327c8U,0xbf597fc7U,0xc6e00bf3U,0xd5a79147U,0x06ca6351U,0x14292967U,
    0x27b70a85U,0x2e1b2138U,0x4d2c6dfcU,0x53380d13U,0x650a7354U,0x766a0abbU,0x81c2c92eU,0x92722c85U,
    0xa2bfe8a1U,0xa81a664bU,0xc24b8b70U,0xc76c51a3U,0xd192e819U,0xd6990624U,0xf40e3585U,0x106aa070U,
    0x19a4c116U,0x1e376c08U,0x2748774cU,0x34b0bcb5U,0x391c0cb3U,0x4ed8aa4aU,0x5b9cca4fU,0x682e6ff3U,
    0x748f82eeU,0x78a5636fU,0x84c87814U,0x8cc70208U,0x90befffaU,0xa4506cebU,0xbef9a3f7U,0xc67178f2U
};

void sha256_oneblock(uchar out[32], const uchar* data, uint len) {
    uint w[64];
    for (int i = 0; i < 16; ++i) w[i] = 0;
    for (uint i = 0; i < len; ++i) {
        w[i >> 2] |= ((uint)data[i]) << (24 - 8 * (i & 3));
    }
    w[len >> 2] |= 0x80U << (24 - 8 * (len & 3));
    w[15] = len * 8U;
    for (int i = 16; i < 64; ++i) {
        w[i] = ssig1(w[i - 2]) + w[i - 7] + ssig0(w[i - 15]) + w[i - 16];
    }

    uint a = 0x6a09e667U, b = 0xbb67ae85U, c = 0x3c6ef372U, d = 0xa54ff53aU;
    uint e = 0x510e527fU, f = 0x9b05688cU, g = 0x1f83d9abU, h = 0x5be0cd19U;
    for (int i = 0; i < 64; ++i) {
        uint t1 = h + bsig1(e) + ch32(e, f, g) + SHA256_K[i] + w[i];
        uint t2 = bsig0(a) + maj32(a, b, c);
        h = g; g = f; f = e; e = d + t1;
        d = c; c = b; b = a; a = t1 + t2;
    }

    uint digest[8] = {
        a + 0x6a09e667U, b + 0xbb67ae85U, c + 0x3c6ef372U, d + 0xa54ff53aU,
        e + 0x510e527fU, f + 0x9b05688cU, g + 0x1f83d9abU, h + 0x5be0cd19U
    };
    for (int i = 0; i < 8; ++i) {
        out[i * 4 + 0] = (uchar)(digest[i] >> 24);
        out[i * 4 + 1] = (uchar)(digest[i] >> 16);
        out[i * 4 + 2] = (uchar)(digest[i] >> 8);
        out[i * 4 + 3] = (uchar)(digest[i]);
    }
}

uint base58check_address(char out[36], const uchar addr[21]) {
    uchar hash1[32];
    uchar hash2[32];
    sha256_oneblock(hash1, addr, 21);
    sha256_oneblock(hash2, hash1, 32);

    uchar data[25];
    for (int i = 0; i < 21; ++i) data[i] = addr[i];
    for (int i = 0; i < 4; ++i) data[21 + i] = hash2[i];

    uchar digits[36];
    for (int i = 0; i < 36; ++i) digits[i] = 0;
    for (int i = 0; i < 25; ++i) {
        uint carry = data[i];
        for (int j = 0; j < 36; ++j) {
            carry += ((uint)digits[j]) * 256U;
            digits[j] = (uchar)(carry % 58U);
            carry /= 58U;
        }
    }

    int digit_start = 36;
    while (digit_start > 0 && digits[digit_start - 1] == 0) --digit_start;
    uint len = 0;
    for (int i = digit_start; i > 0; --i) {
        out[len++] = BASE58_CHARS[digits[i - 1]];
    }
    out[len] = '\0';
    return len;
}

bool gpu_exact_pattern_match(const char* address, uint address_len, uint pattern_type, uint pattern_len, __global const uchar* pattern_chars) {
    if (pattern_len == 0 || pattern_len > 20 || address_len < pattern_len) return false;

    if (pattern_type == 0U) { // suffix
        uint start = address_len - pattern_len;
        for (uint i = 0; i < pattern_len; ++i) {
            if ((uchar)address[start + i] != pattern_chars[i]) return false;
        }
        return true;
    }

    if (pattern_type == 1U) { // prefix after TRX leading 'T'
        if (address_len < pattern_len + 1U) return false;
        for (uint i = 0; i < pattern_len; ++i) {
            if ((uchar)address[1 + i] != pattern_chars[i]) return false;
        }
        return true;
    }

    if (pattern_type == 2U) { // contains anywhere, including leading T
        for (uint start = 0; start + pattern_len <= address_len; ++start) {
            bool ok = true;
            for (uint i = 0; i < pattern_len; ++i) {
                if ((uchar)address[start + i] != pattern_chars[i]) { ok = false; break; }
            }
            if (ok) return true;
        }
        return false;
    }

    return false;
}

__kernel void test_base58check_filter(
    __global const uchar* address_bytes_in,
    __global const uchar* pattern_chars,
    uint pattern_type,
    uint pattern_len,
    __global char* encoded_out,
    __global uint* result_out
) {
    uchar address_bytes[21];
    for (int i = 0; i < 21; ++i) address_bytes[i] = address_bytes_in[i];

    char encoded[36];
    uint encoded_len = base58check_address(encoded, address_bytes);
    bool matched = gpu_exact_pattern_match(encoded, encoded_len, pattern_type, pattern_len, pattern_chars);

    for (int i = 0; i < 36; ++i) encoded_out[i] = encoded[i];
    result_out[0] = encoded_len;
    result_out[1] = matched ? 1U : 0U;
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
    uint batch_size,
    uint pattern_type,
    uint pattern_len,
    __global const uchar* pattern_chars
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

    // Exact GPU-side Base58Check filtering. Only Base58 matches are returned to
    // host; CPU keeps a final verifier for correctness/debugging but is no
    // longer on the per-candidate hot path.
    char address_base58[36];
    uint address_len = base58check_address(address_base58, address_bytes);
    bool is_match = gpu_exact_pattern_match(address_base58, address_len, pattern_type, pattern_len, pattern_chars);

    if (is_match) {
        uint idx = atomic_inc(match_count);
        if (idx < batch_size) {
            results[idx].seed[0] = seeds[gid].x;
            results[idx].seed[1] = seeds[gid].y;
            results[idx].seed[2] = seeds[gid].z;
            results[idx].seed[3] = seeds[gid].w;
            results[idx].match_type = 0;
            results[idx].reserved[0] = gid;
            results[idx].reserved[1] = 0;
            results[idx].reserved[2] = 0;

            // Copy address bytes to output
            for (int i = 0; i < 21; i++) {
                addresses_out[idx * 21 + i] = address_bytes[i];
            }
        }
    }
}

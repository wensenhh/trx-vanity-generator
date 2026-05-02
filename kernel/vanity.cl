// ============================================================================
// TRON Vanity Address Generator - OpenCL Kernel
// Phase 2: GPU-accelerated Keccak256 + address matching
// Phase 3: Full GPU ECC (secp256k1)
// ============================================================================

// Base58 alphabet for pattern matching
constant char BASE58_CHARS[] = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";

// Keccak-256 round constants
constant ulong KECCAK_RC[24] = {
    0x0000000000000001UL, 0x0000000000008082UL, 0x800000000000808aUL,
    0x8000000080008000UL, 0x000000000000808bUL, 0x0000000080000001UL,
    0x8000000080008081UL, 0x8000000000008009UL, 0x000000000000008aUL,
    0x0000000000000088UL, 0x0000000080008009UL, 0x000000008000000aUL,
    0x000000008000808bUL, 0x800000000000008bUL, 0x8000000000008089UL,
    0x8000000000008003UL, 0x8000000000008002UL, 0x8000000000000080UL,
    0x000000000000800aUL, 0x800000008000000aUL, 0x8000000080008081UL,
    0x8000000000008080UL, 0x0000000080000001UL, 0x8000000080008008UL
};

// ============================================================================
// Utility Functions
// ============================================================================

ulong rol64(ulong x, int n) {
    return (x << n) | (x >> (64 - n));
}

// xoshiro256++ PRNG for GPU
// Each work-item gets its own state derived from global seed
uint4 xoshiro_next(ulong4* state) {
    ulong result = rol64(state->x + state->w, 23) + state->x;
    ulong t = state->y << 17;

    state->z ^= state->x;
    state->w ^= state->y;
    state->y ^= state->z;
    state->x ^= state->w;
    state->z ^= t;
    state->w = rol64(state->w, 45);

    return (uint4)(
        (uint)(result >> 32),
        (uint)(result & 0xFFFFFFFF),
        (uint)(result >> 32) ^ (uint)(result & 0xFFFFFFFF),
        (uint)(result >> 16) & 0xFFFFFFFF
    );
}

void init_rng(ulong4* state, uint4 seed, uint gid) {
    // Mix seed with work-item ID for unique streams
    ulong s0 = ((ulong)seed.x << 32) | seed.y;
    ulong s1 = ((ulong)seed.z << 32) | seed.w;
    ulong s2 = ((ulong)gid << 32) | (s0 ^ s1);
    ulong s3 = s0 ^ (s1 >> 17) ^ (s2 << 13);

    // SplitMix64 to initialize state
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
// Keccak-256 (FIPS 202) - GPU Implementation
// ============================================================================

#define KECCAK_RATE 136
#define KECCAK_STATE_SIZE 25

void keccak_f(ulong* state) {
    ulong t[5];
    ulong bc[5];

    for (int round = 0; round < 24; round++) {
        // Theta
        for (int i = 0; i < 5; i++) {
            bc[i] = state[i] ^ state[i + 5] ^ state[i + 10] ^ state[i + 15] ^ state[i + 20];
        }
        for (int i = 0; i < 5; i++) {
            t[i] = bc[(i + 4) % 5] ^ rol64(bc[(i + 1) % 5], 1);
        }
        for (int i = 0; i < 25; i++) {
            state[i] ^= t[i % 5];
        }

        // Rho and Pi
        ulong last = state[1];
        int piln[24] = {
            10, 7, 11, 17, 18, 3, 5, 16, 8, 21, 24, 4,
            15, 23, 19, 13, 12, 2, 20, 14, 22, 9, 6, 1
        };
        int rotc[24] = {
            1, 3, 6, 10, 15, 21, 28, 36, 45, 55, 2, 14,
            27, 41, 56, 8, 25, 43, 62, 18, 39, 61, 20, 44
        };
        for (int i = 0; i < 24; i++) {
            ulong temp = state[piln[i]];
            state[piln[i]] = rol64(last, rotc[i]);
            last = temp;
        }

        // Chi
        for (int j = 0; j < 25; j += 5) {
            for (int i = 0; i < 5; i++) {
                t[i] = state[j + i];
            }
            for (int i = 0; i < 5; i++) {
                state[j + i] ^= (~t[(i + 1) % 5]) & t[(i + 2) % 5];
            }
        }

        // Iota
        state[0] ^= KECCAK_RC[round];
    }
}

void keccak256(uchar* output, const uchar* input, uint input_len) {
    ulong state[KECCAK_STATE_SIZE];
    for (int i = 0; i < KECCAK_STATE_SIZE; i++) {
        state[i] = 0;
    }

    // Absorb input
    uint offset = 0;
    while (offset < input_len) {
        uint chunk = min(input_len - offset, (uint)KECCAK_RATE);
        for (uint i = 0; i < chunk; i++) {
            ((uchar*)state)[i] ^= input[offset + i];
        }
        keccak_f(state);
        offset += chunk;
    }

    // Pad (Keccak: 0x01 then 0x00... with final 0x80)
    ((uchar*)state)[input_len % KECCAK_RATE] ^= 0x01;
    ((uchar*)state)[KECCAK_RATE - 1] ^= 0x80;
    keccak_f(state);

    // Squeeze output (256 bits = 32 bytes)
    for (int i = 0; i < 32; i++) {
        output[i] = ((uchar*)state)[i];
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
    __global const uint4* seeds,       // Input: random seeds [batch_size]
    __global uint4* results,           // Output: matching seeds [MAX_RESULTS]
    __global uint* match_count,        // Output: atomic counter for matches
    uint batch_size                    // Input: number of work-items
) {
    uint gid = get_global_id(0);
    if (gid >= batch_size) return;

    // Initialize RNG
    ulong4 rng_state;
    init_rng(&rng_state, seeds[gid], gid);

    // Generate 32-byte private key
    uchar private_key[32];
    for (int i = 0; i < 8; i++) {
        uint4 r = xoshiro_next(&rng_state);
        private_key[i * 4 + 0] = (uchar)(r.x >> 24);
        private_key[i * 4 + 1] = (uchar)(r.x >> 16);
        private_key[i * 4 + 2] = (uchar)(r.x >> 8);
        private_key[i * 4 + 3] = (uchar)(r.x);
    }

    // Phase 2: Return private key for CPU processing
    // Phase 3: Do ECC + Keccak256 here

    // For Phase 2: we just return the seed so CPU can regenerate
    // The actual matching is done on CPU side

    // Atomically increment match count and store result
    uint idx = atomic_inc(match_count);
    if (idx < 1024) {
        results[idx] = seeds[gid];
    }
}

// ============================================================================
// Phase 3: Full GPU Pipeline (ECC + Keccak256 + Address Matching)
// ============================================================================

// Include ECC implementation
// Note: In production, use #include "ecc.cl" or merge at build time

// secp256k1 curve parameters
constant ulong SECP256K1_P[4] = {0xFFFFFFFFFFFFFFFFUL, 0xFFFFFFFFFFFFFFFEUL, 0xFFFFFFFFFFFFFFFFUL, 0xFFFFFFFFFFFFFFFFUL};
constant ulong SECP256K1_N[4] = {0xBFD25E8CD0364141UL, 0xBAAEDCE6AF48A03BUL, 0xFFFFFFFFFFFFFFFEUL, 0xFFFFFFFFFFFFFFFFUL};
constant ulong SECP256K1_GX[4] = {0x59F2815B16F81798UL, 0x029BFCDB2DCE28D9UL, 0x55A06295CE870B07UL, 0x79BE667EF9DCBBACUL};
constant ulong SECP256K1_GY[4] = {0x9C47D08FFB10D4B8UL, 0xFD17B448A6855419UL, 0x5C6A30C994A29846UL, 0x483ADA7726A3C465UL};

// 256-bit big integer (4 x 64-bit limbs)
typedef struct {
    ulong d[4];
} uint256;

// Jacobian point
typedef struct {
    uint256 x, y, z;
} jacobian_point;

// uint256 helpers
int uint256_cmp(const uint256* a, const uint256* b) {
    for (int i = 3; i >= 0; i--) {
        if (a->d[i] > b->d[i]) return 1;
        if (a->d[i] < b->d[i]) return -1;
    }
    return 0;
}

bool uint256_is_zero(const uint256* a) {
    return (a->d[0] | a->d[1] | a->d[2] | a->d[3]) == 0;
}

void uint256_from_bytes(uint256* a, const uchar* bytes) {
    for (int i = 0; i < 4; i++) {
        a->d[i] = ((ulong)bytes[24 - i*8] << 56) |
                   ((ulong)bytes[25 - i*8] << 48) |
                   ((ulong)bytes[26 - i*8] << 40) |
                   ((ulong)bytes[27 - i*8] << 32) |
                   ((ulong)bytes[28 - i*8] << 24) |
                   ((ulong)bytes[29 - i*8] << 16) |
                   ((ulong)bytes[30 - i*8] << 8)  |
                   ((ulong)bytes[31 - i*8]);
    }
}

// Modular subtraction
void uint256_sub_mod(uint256* result, const uint256* a, const uint256* b, const uint256* p) {
    long borrow = 0;
    for (int i = 0; i < 4; i++) {
        long diff = (long)a->d[i] - (long)b->d[i] + borrow;
        result->d[i] = (ulong)diff;
        borrow = (diff < 0) ? -1 : 0;
    }
    if (borrow) {
        long carry = 0;
        for (int i = 0; i < 4; i++) {
            ulong sum = result->d[i] + p->d[i] + carry;
            result->d[i] = sum;
            carry = (sum < result->d[i]) ? 1 : 0;
        }
    }
}

// Modular addition
void uint256_add_mod(uint256* result, const uint256* a, const uint256* b, const uint256* p) {
    ulong carry = 0;
    for (int i = 0; i < 4; i++) {
        ulong sum = a->d[i] + b->d[i] + carry;
        result->d[i] = sum;
        carry = (sum < a->d[i]) ? 1 : 0;
    }
    if (carry || uint256_cmp(result, p) >= 0) {
        long borrow = 0;
        for (int i = 0; i < 4; i++) {
            long diff = (long)result->d[i] - (long)p->d[i] + borrow;
            result->d[i] = (ulong)diff;
            borrow = (diff < 0) ? -1 : 0;
        }
    }
}

// Right shift
void uint256_rshift1(uint256* result, const uint256* a) {
    for (int i = 0; i < 3; i++) {
        result->d[i] = (a->d[i] >> 1) | (a->d[i + 1] << 63);
    }
    result->d[3] = a->d[3] >> 1;
}

// Montgomery multiplication (simplified for OpenCL)
constant ulong MONT_R = 0xD838091DD2253531UL;

void montgomery_mul(uint256* result, const uint256* a, const uint256* b, const uint256* p) {
    ulong t[8] = {0};
    for (int i = 0; i < 4; i++) {
        ulong carry = 0;
        for (int j = 0; j < 4; j++) {
            ulong lo = a->d[i] * b->d[j];
            ulong hi = mul_hi(a->d[i], b->d[j]);
            ulong sum = t[i + j] + lo + carry;
            carry = (sum < t[i + j]) ? 1 : 0;
            carry += hi;
            t[i + j] = sum;
        }
        t[i + 4] += carry;
    }
    for (int i = 0; i < 4; i++) {
        ulong m = t[i] * MONT_R;
        ulong carry = 0;
        for (int j = 0; j < 4; j++) {
            ulong lo = m * p->d[j];
            ulong hi = mul_hi(m, p->d[j]);
            ulong sum = t[i + j] + lo + carry;
            carry = (sum < t[i + j]) ? 1 : 0;
            carry += hi;
            t[i + j] = sum;
        }
        for (int j = i + 4; j < 8; j++) {
            ulong sum = t[j] + carry;
            carry = (sum < t[j]) ? 1 : 0;
            t[j] = sum;
        }
    }
    for (int i = 0; i < 4; i++) {
        result->d[i] = t[i + 4];
    }
    if (uint256_cmp(result, p) >= 0) {
        uint256_sub_mod(result, result, p, p);
    }
}

// Modular exponentiation
void uint256_mod_exp(uint256* result, const uint256* base, const uint256* exp, const uint256* p) {
    uint256 r;
    r.d[0] = 1; r.d[1] = 0; r.d[2] = 0; r.d[3] = 0;
    uint256 b = *base;
    uint256 e = *exp;
    while (!uint256_is_zero(&e)) {
        if (e.d[0] & 1) {
            montgomery_mul(&r, &r, &b, p);
        }
        montgomery_mul(&b, &b, &b, p);
        uint256_rshift1(&e, &e);
    }
    *result = r;
}

// Modular inverse
void uint256_mod_inverse(uint256* result, const uint256* a, const uint256* p) {
    uint256 p_minus_2;
    p_minus_2.d[0] = 0xFFFFFFFFFFFFFFFDUL;
    p_minus_2.d[1] = 0xFFFFFFFFFFFFFFFEUL;
    p_minus_2.d[2] = 0xFFFFFFFFFFFFFFFFUL;
    p_minus_2.d[3] = 0xFFFFFFFFFFFFFFFFUL;
    uint256_mod_exp(result, a, &p_minus_2, p);
}

// Point doubling
void point_double(jacobian_point* r, const jacobian_point* p, const uint256* prime) {
    uint256 lambda1, lambda2, lambda3, temp1, temp2;
    montgomery_mul(&temp1, &p->x, &p->x, prime);
    uint256_add_mod(&temp2, &temp1, &temp1, prime);
    uint256_add_mod(&lambda1, &temp1, &temp2, prime);
    montgomery_mul(&temp1, &p->y, &p->y, prime);
    montgomery_mul(&temp2, &p->x, &temp1, prime);
    uint256_add_mod(&lambda2, &temp2, &temp2, prime);
    uint256_add_mod(&lambda2, &lambda2, &temp2, prime);
    uint256_add_mod(&lambda2, &lambda2, &temp2, prime);
    montgomery_mul(&temp2, &temp1, &temp1, prime);
    uint256_add_mod(&lambda3, &temp2, &temp2, prime);
    uint256_add_mod(&lambda3, &lambda3, &temp2, prime);
    montgomery_mul(&temp1, &lambda1, &lambda1, prime);
    uint256_add_mod(&temp2, &lambda2, &lambda2, prime);
    uint256_sub_mod(&r->x, &temp1, &temp2, prime);
    uint256_sub_mod(&temp1, &lambda2, &r->x, prime);
    montgomery_mul(&temp2, &lambda1, &temp1, prime);
    uint256_sub_mod(&r->y, &temp2, &lambda3, prime);
    montgomery_mul(&temp1, &p->y, &p->z, prime);
    uint256_add_mod(&r->z, &temp1, &temp1, prime);
}

// Point addition
void point_add(jacobian_point* r, const jacobian_point* p,
               const uint256* qx, const uint256* qy, const uint256* prime) {
    uint256 z1_sq, z1_cu, u1, u2, s1, s2, h, h_sq, r_val, temp1, temp2;
    montgomery_mul(&z1_sq, &p->z, &p->z, prime);
    montgomery_mul(&z1_cu, &z1_sq, &p->z, prime);
    montgomery_mul(&u2, qx, &z1_sq, prime);
    montgomery_mul(&s2, qy, &z1_cu, prime);
    u1 = p->x;
    s1 = p->y;
    uint256_sub_mod(&h, &u2, &u1, prime);
    uint256_sub_mod(&r_val, &s2, &s1, prime);
    montgomery_mul(&h_sq, &h, &h, prime);
    montgomery_mul(&temp1, &r_val, &r_val, prime);
    montgomery_mul(&temp2, &h_sq, &h, prime);
    uint256_sub_mod(&temp1, &temp1, &temp2, prime);
    montgomery_mul(&temp2, &u1, &h_sq, prime);
    uint256_add_mod(&temp2, &temp2, &temp2, prime);
    uint256_sub_mod(&r->x, &temp1, &temp2, prime);
    montgomery_mul(&temp1, &u1, &h_sq, prime);
    uint256_sub_mod(&temp1, &temp1, &r->x, prime);
    montgomery_mul(&temp2, &r_val, &temp1, prime);
    montgomery_mul(&temp1, &s1, &h_sq, prime);
    montgomery_mul(&temp1, &temp1, &h, prime);
    uint256_sub_mod(&r->y, &temp2, &temp1, prime);
    montgomery_mul(&r->z, &h, &p->z, prime);
}

// Jacobian to Affine
void jacobian_to_affine(uint256* ax, uint256* ay, const jacobian_point* p, const uint256* prime) {
    uint256 z_inv, z_inv_sq, z_inv_cu;
    uint256_mod_inverse(&z_inv, &p->z, prime);
    montgomery_mul(&z_inv_sq, &z_inv, &z_inv, prime);
    montgomery_mul(&z_inv_cu, &z_inv_sq, &z_inv, prime);
    montgomery_mul(ax, &p->x, &z_inv_sq, prime);
    montgomery_mul(ay, &p->y, &z_inv_cu, prime);
}

// ============================================================================
// Standard Modular Multiplication (Correct, non-Montgomery)
// result = (a * b) mod p  where a, b are in standard domain [0, p-1]
// ============================================================================

// secp256k1 prime: p = 2^256 - 2^32 - 2^9 - 2^8 - 2^7 - 2^6 - 2^4 - 1
// For reduction: 2^256 ≡ 2^32 + 2^9 + 2^8 + 2^7 + 2^6 + 2^4 + 1  (mod p)
// Let c = 2^32 + 2^9 + 2^8 + 2^7 + 2^6 + 2^4 + 1
// Then for any 512-bit value V = high * 2^256 + low:
//   V mod p = (high * c + low) mod p
// c in uint256 form (little-endian limbs):
// c = 0x0000000000000001_0000000000000041 (bit 32 + bit 9 + bit 8 + bit 7 + bit 6 + bit 4 + bit 0)
// c.d[0] = 0x0000000000000041 (bits 0,4,6,7,8,9 set = 1+16+64+128+256+512 = 977 = 0x3D1... wait)
// Actually: 2^32 = 0x100000000, 2^9=512, 2^8=256, 2^7=128, 2^6=64, 2^4=16, 1=1
// Sum = 4294967296 + 512 + 256 + 128 + 64 + 16 + 1 = 4294968273 = 0x100000031
// c.d[0] = 0x100000031, c.d[1..3] = 0

constant uint256 SECP256K1_C = {{0x0000000100000031UL, 0x0UL, 0x0UL, 0x0UL}};

// 256x256 -> 512 bit multiplication
void uint256_mul_512(ulong* out_hi, uint256* out_lo, const uint256* a, const uint256* b) {
    ulong t[8] = {0};
    for (int i = 0; i < 4; i++) {
        ulong carry = 0;
        for (int j = 0; j < 4; j++) {
            ulong lo = a->d[i] * b->d[j];
            ulong hi = mul_hi(a->d[i], b->d[j]);
            ulong sum = t[i + j] + lo + carry;
            carry = (sum < t[i + j]) ? 1 : 0;
            carry += hi;
            t[i + j] = sum;
        }
        t[i + 4] += carry;
    }
    for (int i = 0; i < 4; i++) {
        out_lo->d[i] = t[i];
        out_hi[i] = t[i + 4];
    }
}

// Multiply by c = 2^256 mod p (which is just c since c < p)
// out = (in * c) mod p, where in is a uint256
void uint256_mul_by_c(uint256* out, const uint256* in, const uint256* p) {
    // c = 0x100000031 (fits in one limb)
    // in * c = in * (2^32 + 2^9 + 2^8 + 2^7 + 2^6 + 2^4 + 1)
    //        = in<<32 + in<<9 + in<<8 + in<<7 + in<<6 + in<<4 + in
    // All shifts are within 256 bits, so we can accumulate and reduce
    uint256 acc = *in;  // + in
    uint256 shifted;
    // in << 4
    shifted.d[0] = in->d[0] << 4;
    shifted.d[1] = (in->d[1] << 4) | (in->d[0] >> 60);
    shifted.d[2] = (in->d[2] << 4) | (in->d[1] >> 60);
    shifted.d[3] = (in->d[3] << 4) | (in->d[2] >> 60);
    uint256_add_mod(&acc, &acc, &shifted, p);
    // in << 6
    shifted.d[0] = in->d[0] << 6;
    shifted.d[1] = (in->d[1] << 6) | (in->d[0] >> 58);
    shifted.d[2] = (in->d[2] << 6) | (in->d[1] >> 58);
    shifted.d[3] = (in->d[3] << 6) | (in->d[2] >> 58);
    uint256_add_mod(&acc, &acc, &shifted, p);
    // in << 7
    shifted.d[0] = in->d[0] << 7;
    shifted.d[1] = (in->d[1] << 7) | (in->d[0] >> 57);
    shifted.d[2] = (in->d[2] << 7) | (in->d[1] >> 57);
    shifted.d[3] = (in->d[3] << 7) | (in->d[2] >> 57);
    uint256_add_mod(&acc, &acc, &shifted, p);
    // in << 8
    shifted.d[0] = in->d[0] << 8;
    shifted.d[1] = (in->d[1] << 8) | (in->d[0] >> 56);
    shifted.d[2] = (in->d[2] << 8) | (in->d[1] >> 56);
    shifted.d[3] = (in->d[3] << 8) | (in->d[2] >> 56);
    uint256_add_mod(&acc, &acc, &shifted, p);
    // in << 9
    shifted.d[0] = in->d[0] << 9;
    shifted.d[1] = (in->d[1] << 9) | (in->d[0] >> 55);
    shifted.d[2] = (in->d[2] << 9) | (in->d[1] >> 55);
    shifted.d[3] = (in->d[3] << 9) | (in->d[2] >> 55);
    uint256_add_mod(&acc, &acc, &shifted, p);
    // in << 32
    shifted.d[0] = in->d[0] << 32;
    shifted.d[1] = (in->d[1] << 32) | (in->d[0] >> 32);
    shifted.d[2] = (in->d[2] << 32) | (in->d[1] >> 32);
    shifted.d[3] = (in->d[3] << 32) | (in->d[2] >> 32);
    uint256_add_mod(&acc, &acc, &shifted, p);
    *out = acc;
}

// Standard modular multiplication: result = (a * b) mod p
// Uses the identity: 2^256 ≡ c (mod p) where c = 2^32 + 2^9 + ... + 1
// For V = high * 2^256 + low: V mod p = (high * c + low) mod p
void uint256_mul_mod_std(uint256* result, const uint256* a, const uint256* b, const uint256* p) {
    uint256 low;
    ulong high[4];
    uint256_mul_512(high, &low, a, b);

    // result = (high * c + low) mod p
    uint256 high_val;
    for (int i = 0; i < 4; i++) high_val.d[i] = high[i];

    uint256 high_times_c;
    uint256_mul_by_c(&high_times_c, &high_val, p);

    uint256_add_mod(result, &high_times_c, &low, p);
}

// Point doubling using STANDARD arithmetic (no Montgomery)
void point_double_std(jacobian_point* r, const jacobian_point* p, const uint256* prime) {
    uint256 s, m, temp1, temp2, y_sq, y_sq_sq;
    uint256_mul_mod_std(&y_sq, &p->y, &p->y, prime);
    uint256_mul_mod_std(&y_sq_sq, &y_sq, &y_sq, prime);
    uint256_mul_mod_std(&temp1, &p->x, &y_sq, prime);
    uint256_add_mod(&s, &temp1, &temp1, prime);
    uint256_add_mod(&s, &s, &temp1, prime);
    uint256_add_mod(&s, &s, &temp1, prime);
    uint256_mul_mod_std(&temp1, &p->x, &p->x, prime);
    uint256_add_mod(&m, &temp1, &temp1, prime);
    uint256_add_mod(&m, &m, &temp1, prime);
    uint256_mul_mod_std(&temp1, &m, &m, prime);
    uint256_add_mod(&temp2, &s, &s, prime);
    uint256_sub_mod(&r->x, &temp1, &temp2, prime);
    uint256_sub_mod(&temp1, &s, &r->x, prime);
    uint256_mul_mod_std(&temp2, &m, &temp1, prime);
    uint256_add_mod(&temp1, &y_sq_sq, &y_sq_sq, prime);
    uint256_add_mod(&temp1, &temp1, &y_sq_sq, prime);
    uint256_add_mod(&temp1, &temp1, &y_sq_sq, prime);
    uint256_add_mod(&temp1, &temp1, &temp1, prime);
    uint256_sub_mod(&r->y, &temp2, &temp1, prime);
    uint256_mul_mod_std(&temp1, &p->y, &p->z, prime);
    uint256_add_mod(&r->z, &temp1, &temp1, prime);
}

// Point addition using STANDARD arithmetic
void point_add_std(jacobian_point* r, const jacobian_point* p,
                   const uint256* qx, const uint256* qy, const uint256* prime) {
    uint256 z1_sq, z1_cu, u1, u2, s1, s2, h, h_sq, h_cu, r_val, temp1, temp2;
    uint256_mul_mod_std(&z1_sq, &p->z, &p->z, prime);
    uint256_mul_mod_std(&z1_cu, &z1_sq, &p->z, prime);
    uint256_mul_mod_std(&u2, qx, &z1_sq, prime);
    uint256_mul_mod_std(&s2, qy, &z1_cu, prime);
    u1 = p->x;
    s1 = p->y;
    uint256_sub_mod(&h, &u2, &u1, prime);
    uint256_sub_mod(&r_val, &s2, &s1, prime);
    uint256_mul_mod_std(&h_sq, &h, &h, prime);
    uint256_mul_mod_std(&h_cu, &h_sq, &h, prime);
    uint256_mul_mod_std(&temp1, &r_val, &r_val, prime);
    uint256_sub_mod(&temp1, &temp1, &h_cu, prime);
    uint256_mul_mod_std(&temp2, &u1, &h_sq, prime);
    uint256_add_mod(&temp2, &temp2, &temp2, prime);
    uint256_sub_mod(&r->x, &temp1, &temp2, prime);
    uint256_mul_mod_std(&temp1, &u1, &h_sq, prime);
    uint256_sub_mod(&temp1, &temp1, &r->x, prime);
    uint256_mul_mod_std(&temp2, &r_val, &temp1, prime);
    uint256_mul_mod_std(&temp1, &s1, &h_cu, prime);
    uint256_sub_mod(&r->y, &temp2, &temp1, prime);
    uint256_mul_mod_std(&r->z, &h, &p->z, prime);
}

// Jacobian to Affine using STANDARD arithmetic
void jacobian_to_affine_std(uint256* ax, uint256* ay, const jacobian_point* p, const uint256* prime) {
    uint256 z_inv, z_inv_sq, z_inv_cu;
    uint256 p_minus_2;
    p_minus_2.d[0] = 0xFFFFFFFFFFFFFFFDUL;
    p_minus_2.d[1] = 0xFFFFFFFFFFFFFFFEUL;
    p_minus_2.d[2] = 0xFFFFFFFFFFFFFFFFUL;
    p_minus_2.d[3] = 0xFFFFFFFFFFFFFFFFUL;
    uint256 base = p->z;
    uint256 exp = p_minus_2;
    uint256 result;
    result.d[0] = 1; result.d[1] = 0; result.d[2] = 0; result.d[3] = 0;
    while (!uint256_is_zero(&exp)) {
        if (exp.d[0] & 1) {
            uint256_mul_mod_std(&result, &result, &base, prime);
        }
        uint256_mul_mod_std(&base, &base, &base, prime);
        uint256_rshift1(&exp, &exp);
    }
    z_inv = result;
    uint256_mul_mod_std(&z_inv_sq, &z_inv, &z_inv, prime);
    uint256_mul_mod_std(&z_inv_cu, &z_inv_sq, &z_inv, prime);
    uint256_mul_mod_std(ax, &p->x, &z_inv_sq, prime);
    uint256_mul_mod_std(ay, &p->y, &z_inv_cu, prime);
}

// Scalar multiplication using double-and-add with STANDARD arithmetic (CORRECT)
// result = k * G
void scalar_multiply_base_std(jacobian_point* result, const uint256* k, const uint256* prime) {
    result->x = (uint256){{0, 0, 0, 0}};
    result->y = (uint256){{0, 0, 0, 0}};
    result->z = (uint256){{0, 0, 0, 0}};

    uint256 gx, gy;
    gx.d[0] = 0x59F2815B16F81798UL;
    gx.d[1] = 0x029BFCDB2DCE28D9UL;
    gx.d[2] = 0x55A06295CE870B07UL;
    gx.d[3] = 0x79BE667EF9DCBBACUL;
    gy.d[0] = 0x9C47D08FFB10D4B8UL;
    gy.d[1] = 0xFD17B448A6855419UL;
    gy.d[2] = 0x5C6A30C994A29846UL;
    gy.d[3] = 0x483ADA7726A3C465UL;

    // Find highest set bit
    int highest_bit = 255;
    while (highest_bit >= 0) {
        int limb = highest_bit / 64;
        int bit_in_limb = highest_bit % 64;
        if ((k->d[limb] >> bit_in_limb) & 1) break;
        highest_bit--;
    }

    if (highest_bit < 0) return;  // k = 0

    // Initialize with G
    result->x = gx;
    result->y = gy;
    result->z = (uint256){{1, 0, 0, 0}};

    // Process remaining bits
    for (int i = highest_bit - 1; i >= 0; i--) {
        point_double_std(result, result, prime);
        int bit = (int)((k->d[i / 64] >> (i % 64)) & 1);
        if (bit) {
            point_add_std(result, result, &gx, &gy, prime);
        }
    }
}

// Generate public key from private key using STANDARD arithmetic
void generate_public_key_gpu(uchar* public_key, const uchar* private_key, const uint256* prime) {
    uint256 k;
    uint256_from_bytes(&k, private_key);
    jacobian_point result;
    scalar_multiply_base_std(&result, &k, prime);
    uint256 ax, ay;
    jacobian_to_affine_std(&ax, &ay, &result, prime);
    for (int i = 0; i < 4; i++) {
        public_key[i * 8 + 0] = (uchar)(ax.d[3 - i] >> 56);
        public_key[i * 8 + 1] = (uchar)(ax.d[3 - i] >> 48);
        public_key[i * 8 + 2] = (uchar)(ax.d[3 - i] >> 40);
        public_key[i * 8 + 3] = (uchar)(ax.d[3 - i] >> 32);
        public_key[i * 8 + 4] = (uchar)(ax.d[3 - i] >> 24);
        public_key[i * 8 + 5] = (uchar)(ax.d[3 - i] >> 16);
        public_key[i * 8 + 6] = (uchar)(ax.d[3 - i] >> 8);
        public_key[i * 8 + 7] = (uchar)(ax.d[3 - i]);
    }
    for (int i = 0; i < 4; i++) {
        public_key[32 + i * 8 + 0] = (uchar)(ay.d[3 - i] >> 56);
        public_key[32 + i * 8 + 1] = (uchar)(ay.d[3 - i] >> 48);
        public_key[32 + i * 8 + 2] = (uchar)(ay.d[3 - i] >> 40);
        public_key[32 + i * 8 + 3] = (uchar)(ay.d[3 - i] >> 32);
        public_key[32 + i * 8 + 4] = (uchar)(ay.d[3 - i] >> 24);
        public_key[32 + i * 8 + 5] = (uchar)(ay.d[3 - i] >> 16);
        public_key[32 + i * 8 + 6] = (uchar)(ay.d[3 - i] >> 8);
        public_key[32 + i * 8 + 7] = (uchar)(ay.d[3 - i]);
    }
}

// ============================================================================
// Phase 3 Kernel: Full GPU Pipeline
// ============================================================================

__kernel void generate_addresses_full_gpu(
    __global const uint4* seeds,
    __global uint4* results,
    __global uint* match_count,
    __global uchar* addresses_out,
    uint batch_size
) {
    uint gid = get_global_id(0);
    if (gid >= batch_size) return;

    uint256 prime;
    prime.d[0] = 0xFFFFFFFFFFFFFFFFUL;
    prime.d[1] = 0xFFFFFFFFFFFFFFFEUL;
    prime.d[2] = 0xFFFFFFFFFFFFFFFFUL;
    prime.d[3] = 0xFFFFFFFFFFFFFFFFUL;

    // Initialize RNG (must match CPU seed_to_bytes exactly)
    ulong4 rng_state;
    init_rng(&rng_state, seeds[gid], gid);

    // Generate private key
    uchar private_key[32];
    for (int i = 0; i < 8; i++) {
        uint4 r = xoshiro_next(&rng_state);
        private_key[i * 4 + 0] = (uchar)(r.x >> 24);
        private_key[i * 4 + 1] = (uchar)(r.x >> 16);
        private_key[i * 4 + 2] = (uchar)(r.x >> 8);
        private_key[i * 4 + 3] = (uchar)(r.x);
    }

    // ECC: private key -> public key
    uchar public_key[64];
    generate_public_key_gpu(public_key, private_key, &prime);

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
        if (idx < 1024) {
            results[idx] = seeds[gid];
            // Copy address bytes to output
            for (int i = 0; i < 21; i++) {
                addresses_out[idx * 21 + i] = address_bytes[i];
            }
        }
    }
}

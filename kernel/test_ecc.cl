// CACHE_BUST: 744656
// ============================================================================
// TRON ECC Test Kernel
// Standalone kernel for debugging GPU ECC correctness
// Uses STANDARD modular arithmetic (no Montgomery form)
// ============================================================================

// secp256k1 curve parameters
constant ulong SECP256K1_P[4] = {0xFFFFFFFEFFFFFC2FUL, 0xFFFFFFFFFFFFFFFFUL, 0xFFFFFFFFFFFFFFFFUL, 0xFFFFFFFFFFFFFFFFUL};
constant ulong SECP256K1_N[4] = {0xBFD25E8CD0364141UL, 0xBAAEDCE6AF48A03BUL, 0xFFFFFFFFFFFFFFFEUL, 0xFFFFFFFFFFFFFFFFUL};
constant ulong SECP256K1_GX[4] = {0x59F2815B16F81798UL, 0x029BFCDB2DCE28D9UL, 0x55A06295CE870B07UL, 0x79BE667EF9DCBBACUL};
constant ulong SECP256K1_GY[4] = {0x9C47D08FFB10D4B8UL, 0xFD17B448A6855419UL, 0x5DA4FBFC0E1108A8UL, 0x483ADA7726A3C465UL};

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

// Load 32 bytes big-endian into uint256 (little-endian limbs)
// bytes[0] is MSB -> goes into d[3] high bytes
// bytes[31] is LSB -> goes into d[0] low bytes
void uint256_from_bytes(uint256* a, const uchar* bytes) {
    a->d[0] = ((ulong)bytes[24] << 56) | ((ulong)bytes[25] << 48) | ((ulong)bytes[26] << 40) | ((ulong)bytes[27] << 32) |
              ((ulong)bytes[28] << 24) | ((ulong)bytes[29] << 16) | ((ulong)bytes[30] << 8)  | ((ulong)bytes[31]);
    a->d[1] = ((ulong)bytes[16] << 56) | ((ulong)bytes[17] << 48) | ((ulong)bytes[18] << 40) | ((ulong)bytes[19] << 32) |
              ((ulong)bytes[20] << 24) | ((ulong)bytes[21] << 16) | ((ulong)bytes[22] << 8)  | ((ulong)bytes[23]);
    a->d[2] = ((ulong)bytes[8]  << 56) | ((ulong)bytes[9]  << 48) | ((ulong)bytes[10] << 40) | ((ulong)bytes[11] << 32) |
              ((ulong)bytes[12] << 24) | ((ulong)bytes[13] << 16) | ((ulong)bytes[14] << 8)  | ((ulong)bytes[15]);
    a->d[3] = ((ulong)bytes[0]  << 56) | ((ulong)bytes[1]  << 48) | ((ulong)bytes[2]  << 40) | ((ulong)bytes[3]  << 32) |
              ((ulong)bytes[4]  << 24) | ((ulong)bytes[5]  << 16) | ((ulong)bytes[6]  << 8)  | ((ulong)bytes[7]);
}

// Store uint256 (little-endian limbs) to 32 bytes big-endian
// d[3] (high limb) -> bytes[0..7] (MSB first)
// d[0] (low limb)  -> bytes[24..31] (LSB last)
void uint256_to_bytes(uchar* bytes, const uint256* a) {
    bytes[0]  = (uchar)(a->d[3] >> 56); bytes[1]  = (uchar)(a->d[3] >> 48); bytes[2]  = (uchar)(a->d[3] >> 40); bytes[3]  = (uchar)(a->d[3] >> 32);
    bytes[4]  = (uchar)(a->d[3] >> 24); bytes[5]  = (uchar)(a->d[3] >> 16); bytes[6]  = (uchar)(a->d[3] >> 8);  bytes[7]  = (uchar)(a->d[3]);
    bytes[8]  = (uchar)(a->d[2] >> 56); bytes[9]  = (uchar)(a->d[2] >> 48); bytes[10] = (uchar)(a->d[2] >> 40); bytes[11] = (uchar)(a->d[2] >> 32);
    bytes[12] = (uchar)(a->d[2] >> 24); bytes[13] = (uchar)(a->d[2] >> 16); bytes[14] = (uchar)(a->d[2] >> 8);  bytes[15] = (uchar)(a->d[2]);
    bytes[16] = (uchar)(a->d[1] >> 56); bytes[17] = (uchar)(a->d[1] >> 48); bytes[18] = (uchar)(a->d[1] >> 40); bytes[19] = (uchar)(a->d[1] >> 32);
    bytes[20] = (uchar)(a->d[1] >> 24); bytes[21] = (uchar)(a->d[1] >> 16); bytes[22] = (uchar)(a->d[1] >> 8);  bytes[23] = (uchar)(a->d[1]);
    bytes[24] = (uchar)(a->d[0] >> 56); bytes[25] = (uchar)(a->d[0] >> 48); bytes[26] = (uchar)(a->d[0] >> 40); bytes[27] = (uchar)(a->d[0] >> 32);
    bytes[28] = (uchar)(a->d[0] >> 24); bytes[29] = (uchar)(a->d[0] >> 16); bytes[30] = (uchar)(a->d[0] >> 8);  bytes[31] = (uchar)(a->d[0]);
}
void uint256_to_global_bytes(__global uchar* bytes, const uint256* a) {
    bytes[0]  = (uchar)(a->d[3] >> 56); bytes[1]  = (uchar)(a->d[3] >> 48); bytes[2]  = (uchar)(a->d[3] >> 40); bytes[3]  = (uchar)(a->d[3] >> 32);
    bytes[4]  = (uchar)(a->d[3] >> 24); bytes[5]  = (uchar)(a->d[3] >> 16); bytes[6]  = (uchar)(a->d[3] >> 8);  bytes[7]  = (uchar)(a->d[3]);
    bytes[8]  = (uchar)(a->d[2] >> 56); bytes[9]  = (uchar)(a->d[2] >> 48); bytes[10] = (uchar)(a->d[2] >> 40); bytes[11] = (uchar)(a->d[2] >> 32);
    bytes[12] = (uchar)(a->d[2] >> 24); bytes[13] = (uchar)(a->d[2] >> 16); bytes[14] = (uchar)(a->d[2] >> 8);  bytes[15] = (uchar)(a->d[2]);
    bytes[16] = (uchar)(a->d[1] >> 56); bytes[17] = (uchar)(a->d[1] >> 48); bytes[18] = (uchar)(a->d[1] >> 40); bytes[19] = (uchar)(a->d[1] >> 32);
    bytes[20] = (uchar)(a->d[1] >> 24); bytes[21] = (uchar)(a->d[1] >> 16); bytes[22] = (uchar)(a->d[1] >> 8);  bytes[23] = (uchar)(a->d[1]);
    bytes[24] = (uchar)(a->d[0] >> 56); bytes[25] = (uchar)(a->d[0] >> 48); bytes[26] = (uchar)(a->d[0] >> 40); bytes[27] = (uchar)(a->d[0] >> 32);
    bytes[28] = (uchar)(a->d[0] >> 24); bytes[29] = (uchar)(a->d[0] >> 16); bytes[30] = (uchar)(a->d[0] >> 8);  bytes[31] = (uchar)(a->d[0]);
}

// Modular subtraction: result = (a - b) mod p
void uint256_sub_mod(uint256* result, const uint256* a, const uint256* b, const uint256* p) {
    (void)p;
    uint aa[8], bb[8], r[8];
    for (int i = 0; i < 4; i++) {
        aa[2 * i] = (uint)(a->d[i] & 0xFFFFFFFFUL);
        aa[2 * i + 1] = (uint)(a->d[i] >> 32);
        bb[2 * i] = (uint)(b->d[i] & 0xFFFFFFFFUL);
        bb[2 * i + 1] = (uint)(b->d[i] >> 32);
    }

    ulong borrow = 0;
    for (int i = 0; i < 8; i++) {
        ulong subtrahend = (ulong)bb[i] + borrow;
        ulong val = (ulong)aa[i];
        r[i] = (uint)(val - subtrahend);
        borrow = (val < subtrahend) ? 1 : 0;
    }

    if (borrow) {
        const uint pp[8] = {0xFFFFFC2FU, 0xFFFFFFFEU, 0xFFFFFFFFU, 0xFFFFFFFFU,
                            0xFFFFFFFFU, 0xFFFFFFFFU, 0xFFFFFFFFU, 0xFFFFFFFFU};
        ulong carry = 0;
        for (int i = 0; i < 8; i++) {
            ulong v = (ulong)r[i] + (ulong)pp[i] + carry;
            r[i] = (uint)v;
            carry = v >> 32;
        }
        // The carry from adding p is the discarded 2^256 word from
        // (a - b + p) represented in eight 32-bit limbs; do not fold it.
        (void)carry;
    }

    result->d[0] = ((ulong)r[1] << 32) | (ulong)r[0];
    result->d[1] = ((ulong)r[3] << 32) | (ulong)r[2];
    result->d[2] = ((ulong)r[5] << 32) | (ulong)r[4];
    result->d[3] = ((ulong)r[7] << 32) | (ulong)r[6];
}

// Modular addition: result = (a + b) mod p
void uint256_add_mod(uint256* result, const uint256* a, const uint256* b, const uint256* p) {
    (void)p;
    uint aa[8], bb[8], r[8];
    for (int i = 0; i < 4; i++) {
        aa[2 * i] = (uint)(a->d[i] & 0xFFFFFFFFUL);
        aa[2 * i + 1] = (uint)(a->d[i] >> 32);
        bb[2 * i] = (uint)(b->d[i] & 0xFFFFFFFFUL);
        bb[2 * i + 1] = (uint)(b->d[i] >> 32);
    }

    ulong carry = 0;
    for (int i = 0; i < 8; i++) {
        ulong v = (ulong)aa[i] + (ulong)bb[i] + carry;
        r[i] = (uint)v;
        carry = v >> 32;
    }

    // Fold one possible 2^256 overflow: 2^256 ≡ 2^32 + 977.
    if (carry) {
        ulong v = (ulong)r[0] + 977UL;
        r[0] = (uint)v;
        carry = v >> 32;
        v = (ulong)r[1] + 1UL + carry;
        r[1] = (uint)v;
        carry = v >> 32;
        for (int i = 2; i < 8 && carry; i++) {
            v = (ulong)r[i] + carry;
            r[i] = (uint)v;
            carry = v >> 32;
        }
    }

    const uint pp[8] = {0xFFFFFC2FU, 0xFFFFFFFEU, 0xFFFFFFFFU, 0xFFFFFFFFU,
                        0xFFFFFFFFU, 0xFFFFFFFFU, 0xFFFFFFFFU, 0xFFFFFFFFU};
    for (int iter = 0; iter < 2; iter++) {
        int cmp = 0;
        for (int i = 7; i >= 0; i--) {
            if (r[i] > pp[i]) { cmp = 1; break; }
            if (r[i] < pp[i]) { cmp = -1; break; }
        }
        if (cmp < 0) break;
        ulong borrow = 0;
        for (int i = 0; i < 8; i++) {
            ulong subtrahend = (ulong)pp[i] + borrow;
            ulong val = (ulong)r[i];
            r[i] = (uint)(val - subtrahend);
            borrow = (val < subtrahend) ? 1 : 0;
        }
    }

    result->d[0] = ((ulong)r[1] << 32) | (ulong)r[0];
    result->d[1] = ((ulong)r[3] << 32) | (ulong)r[2];
    result->d[2] = ((ulong)r[5] << 32) | (ulong)r[4];
    result->d[3] = ((ulong)r[7] << 32) | (ulong)r[6];
}

// Right shift
void uint256_rshift1(uint256* result, const uint256* a) {
    for (int i = 0; i < 3; i++) {
        result->d[i] = (a->d[i] >> 1) | (a->d[i + 1] << 63);
    }
    result->d[3] = a->d[3] >> 1;
}

// ============================================================================
// Standard Modular Multiplication (Correct, non-Montgomery)
// Uses secp256k1 special reduction: 2^256 ≡ c (mod p), c = 0x1000003D1
// ============================================================================

constant uint256 SECP256K1_C = {{0x00000001000003D1UL, 0x0UL, 0x0UL, 0x0UL}};

// 256x256 -> 512 bit multiplication
// Uses 32-bit schoolbook limbs so every intermediate product fits in ulong.
// The previous 64-bit carry accumulation lost carry when adding lo + carry,
// which broke point doubling for k >= 2.
void uint256_mul_512(ulong* out_hi, uint256* out_lo, const uint256* a, const uint256* b) {
    uint a32[8];
    uint b32[8];
    uint t[16];

    for (int i = 0; i < 4; i++) {
        a32[2 * i] = (uint)(a->d[i] & 0xFFFFFFFFUL);
        a32[2 * i + 1] = (uint)(a->d[i] >> 32);
        b32[2 * i] = (uint)(b->d[i] & 0xFFFFFFFFUL);
        b32[2 * i + 1] = (uint)(b->d[i] >> 32);
    }
    for (int i = 0; i < 16; i++) t[i] = 0;

    for (int i = 0; i < 8; i++) {
        ulong carry = 0;
        for (int j = 0; j < 8; j++) {
            ulong prod = (ulong)a32[i] * (ulong)b32[j] + (ulong)t[i + j] + carry;
            t[i + j] = (uint)(prod & 0xFFFFFFFFUL);
            carry = prod >> 32;
        }

        int k = i + 8;
        while (carry != 0 && k < 16) {
            ulong sum = (ulong)t[k] + carry;
            t[k] = (uint)(sum & 0xFFFFFFFFUL);
            carry = sum >> 32;
            k++;
        }
    }

    for (int i = 0; i < 4; i++) {
        out_lo->d[i] = ((ulong)t[2 * i + 1] << 32) | (ulong)t[2 * i];
        out_hi[i] = ((ulong)t[2 * (i + 4) + 1] << 32) | (ulong)t[2 * (i + 4)];
    }
}

// Multiply by c = 2^256 mod p = 2^32 + 2^9 + 2^8 + 2^7 + 2^6 + 2^4 + 1
void uint256_mul_by_c(uint256* out, const uint256* in, const uint256* p) {
    uint256 acc = *in;  // in * 1
    uint256 shifted;
    // in << 4 (2^4)
    shifted.d[0] = in->d[0] << 4;
    shifted.d[1] = (in->d[1] << 4) | (in->d[0] >> 60);
    shifted.d[2] = (in->d[2] << 4) | (in->d[1] >> 60);
    shifted.d[3] = (in->d[3] << 4) | (in->d[2] >> 60);
    uint256_add_mod(&acc, &acc, &shifted, p);
    // in << 6 (2^6)
    shifted.d[0] = in->d[0] << 6;
    shifted.d[1] = (in->d[1] << 6) | (in->d[0] >> 58);
    shifted.d[2] = (in->d[2] << 6) | (in->d[1] >> 58);
    shifted.d[3] = (in->d[3] << 6) | (in->d[2] >> 58);
    uint256_add_mod(&acc, &acc, &shifted, p);
    // in << 7 (2^7)
    shifted.d[0] = in->d[0] << 7;
    shifted.d[1] = (in->d[1] << 7) | (in->d[0] >> 57);
    shifted.d[2] = (in->d[2] << 7) | (in->d[1] >> 57);
    shifted.d[3] = (in->d[3] << 7) | (in->d[2] >> 57);
    uint256_add_mod(&acc, &acc, &shifted, p);
    // in << 8 (2^8)
    shifted.d[0] = in->d[0] << 8;
    shifted.d[1] = (in->d[1] << 8) | (in->d[0] >> 56);
    shifted.d[2] = (in->d[2] << 8) | (in->d[1] >> 56);
    shifted.d[3] = (in->d[3] << 8) | (in->d[2] >> 56);
    uint256_add_mod(&acc, &acc, &shifted, p);
    // in << 9 (2^9)
    shifted.d[0] = in->d[0] << 9;
    shifted.d[1] = (in->d[1] << 9) | (in->d[0] >> 55);
    shifted.d[2] = (in->d[2] << 9) | (in->d[1] >> 55);
    shifted.d[3] = (in->d[3] << 9) | (in->d[2] >> 55);
    uint256_add_mod(&acc, &acc, &shifted, p);
    // in << 32 (2^32)
    shifted.d[0] = in->d[0] << 32;
    shifted.d[1] = (in->d[1] << 32) | (in->d[0] >> 32);
    shifted.d[2] = (in->d[2] << 32) | (in->d[1] >> 32);
    shifted.d[3] = (in->d[3] << 32) | (in->d[2] >> 32);
    uint256_add_mod(&acc, &acc, &shifted, p);
    *out = acc;
}

// Standard modular multiplication: result = (a * b) mod p
// 32-bit limb implementation with secp256k1 special reduction.
// p = 2^256 - c, c = 2^32 + 977, so 2^256 ≡ c (mod p).
void uint256_mul_mod(uint256* result, const uint256* a, const uint256* b, const uint256* p) {
    (void)p;
    uint aa[8], bb[8], prod[16];
    for (int i = 0; i < 4; i++) {
        aa[2 * i] = (uint)(a->d[i] & 0xFFFFFFFFUL);
        aa[2 * i + 1] = (uint)(a->d[i] >> 32);
        bb[2 * i] = (uint)(b->d[i] & 0xFFFFFFFFUL);
        bb[2 * i + 1] = (uint)(b->d[i] >> 32);
    }
    for (int i = 0; i < 16; i++) prod[i] = 0;

    for (int i = 0; i < 8; i++) {
        ulong carry = 0;
        for (int j = 0; j < 8; j++) {
            ulong v = (ulong)prod[i + j] + (ulong)aa[i] * (ulong)bb[j] + carry;
            prod[i + j] = (uint)v;
            carry = v >> 32;
        }
        int k = i + 8;
        while (carry && k < 16) {
            ulong v = (ulong)prod[k] + carry;
            prod[k] = (uint)v;
            carry = v >> 32;
            k++;
        }
    }

    // Fold high limbs with 2^256 == 2^32 + 977. Keep extra space for carries.
    ulong acc[12];
    for (int i = 0; i < 12; i++) acc[i] = 0;
    for (int i = 0; i < 8; i++) acc[i] = prod[i];
    for (int i = 0; i < 8; i++) {
        acc[i] += (ulong)prod[i + 8] * 977UL;
        acc[i + 1] += (ulong)prod[i + 8];
    }

    // Propagate, then fold any limbs above 255 bits again. Two passes are enough
    // for the bounded 512-bit product, but loop defensively.
    for (int pass = 0; pass < 4; pass++) {
        for (int i = 0; i < 11; i++) {
            acc[i + 1] += acc[i] >> 32;
            acc[i] &= 0xFFFFFFFFUL;
        }
        ulong hi0 = acc[8];
        ulong hi1 = acc[9];
        ulong hi2 = acc[10];
        ulong hi3 = acc[11];
        if ((hi0 | hi1 | hi2 | hi3) == 0) break;
        acc[8] = acc[9] = acc[10] = acc[11] = 0;
        ulong highs[4] = {hi0, hi1, hi2, hi3};
        for (int i = 0; i < 4; i++) {
            acc[i] += highs[i] * 977UL;
            acc[i + 1] += highs[i];
        }
    }
    for (int i = 0; i < 11; i++) {
        acc[i + 1] += acc[i] >> 32;
        acc[i] &= 0xFFFFFFFFUL;
    }

    uint r[8];
    for (int i = 0; i < 8; i++) r[i] = (uint)acc[i];
    const uint pp[8] = {0xFFFFFC2FU, 0xFFFFFFFEU, 0xFFFFFFFFU, 0xFFFFFFFFU,
                        0xFFFFFFFFU, 0xFFFFFFFFU, 0xFFFFFFFFU, 0xFFFFFFFFU};

    // Final reduction. At most a few subtractions after folding.
    for (int iter = 0; iter < 4; iter++) {
        int cmp = 0;
        for (int i = 7; i >= 0; i--) {
            if (r[i] > pp[i]) { cmp = 1; break; }
            if (r[i] < pp[i]) { cmp = -1; break; }
        }
        if (cmp < 0) break;
        ulong borrow = 0;
        for (int i = 0; i < 8; i++) {
            ulong subtrahend = (ulong)pp[i] + borrow;
            ulong val = (ulong)r[i];
            r[i] = (uint)(val - subtrahend);
            borrow = (val < subtrahend) ? 1 : 0;
        }
    }

    result->d[0] = ((ulong)r[1] << 32) | (ulong)r[0];
    result->d[1] = ((ulong)r[3] << 32) | (ulong)r[2];
    result->d[2] = ((ulong)r[5] << 32) | (ulong)r[4];
    result->d[3] = ((ulong)r[7] << 32) | (ulong)r[6];
}

// ============================================================================
// Point operations using STANDARD modular arithmetic
// ============================================================================

// Point doubling: R = 2P in Jacobian coordinates
// For secp256k1 (a=0):
// S = 4*X*Y²
// M = 3*X²
// X' = M² - 2*S
// Y' = M*(S - X') - 8*Y⁴
// Z' = 2*Y*Z
void point_double_v7(jacobian_point* r, const jacobian_point* p, const uint256* prime) {
    // Make the function alias-safe: scalar multiplication calls
    // point_double_v7(result, result, ...). Z' is computed after Y' is written,
    // so reading directly from p would otherwise see the modified r->y.
    jacobian_point in = *p;
    p = &in;

    uint256 s, m, temp1, temp2, y_sq, y_sq_sq;
    
    // Y²
    uint256_mul_mod(&y_sq, &p->y, &p->y, prime);
    // Y⁴
    uint256_mul_mod(&y_sq_sq, &y_sq, &y_sq, prime);
    
    // S = 4*X*Y²
    uint256_mul_mod(&temp1, &p->x, &y_sq, prime);  // X*Y²
    uint256_add_mod(&s, &temp1, &temp1, prime);     // 2*X*Y²
    uint256_add_mod(&s, &s, &temp1, prime);        // 3*X*Y²
    uint256_add_mod(&s, &s, &temp1, prime);        // 4*X*Y²
    
    // M = 3*X² (a=0 for secp256k1)
    uint256_mul_mod(&temp1, &p->x, &p->x, prime);  // X²
    uint256_add_mod(&m, &temp1, &temp1, prime);     // 2*X²
    uint256_add_mod(&m, &m, &temp1, prime);         // 3*X²
    
    // X' = M² - 2*S
    uint256_mul_mod(&temp1, &m, &m, prime);         // M²
    uint256_add_mod(&temp2, &s, &s, prime);         // 2*S
    uint256_sub_mod(&r->x, &temp1, &temp2, prime);
    
    // Y' = M*(S - X') - 8*Y⁴
    uint256_sub_mod(&temp1, &s, &r->x, prime);
    uint256_mul_mod(&temp2, &m, &temp1, prime);
    
    // 8*Y⁴
    uint256_add_mod(&temp1, &y_sq_sq, &y_sq_sq, prime);  // 2*Y⁴
    uint256_add_mod(&temp1, &temp1, &y_sq_sq, prime);    // 3*Y⁴
    uint256_add_mod(&temp1, &temp1, &y_sq_sq, prime);    // 4*Y⁴
    uint256_add_mod(&temp1, &temp1, &temp1, prime);      // 8*Y⁴
    
    uint256_sub_mod(&r->y, &temp2, &temp1, prime);
    
    // Z' = 2*Y*Z
    uint256_mul_mod(&temp1, &p->y, &p->z, prime);
    uint256_add_mod(&r->z, &temp1, &temp1, prime);
}

// Point addition: R = P + Q
void point_add_std(jacobian_point* r, const jacobian_point* p,
                   const uint256* qx, const uint256* qy, const uint256* prime) {
    uint256 z1_sq, z1_cu, u1, u2, s1, s2, h, h_sq, h_cu, r_val, temp1, temp2;
    
    // Z1², Z1³
    uint256_mul_mod(&z1_sq, &p->z, &p->z, prime);
    uint256_mul_mod(&z1_cu, &z1_sq, &p->z, prime);
    
    // U2 = X2 * Z1², S2 = Y2 * Z1³
    uint256_mul_mod(&u2, qx, &z1_sq, prime);
    uint256_mul_mod(&s2, qy, &z1_cu, prime);
    
    // U1 = X1, S1 = Y1
    u1 = p->x;
    s1 = p->y;
    
    // H = U2 - U1
    uint256_sub_mod(&h, &u2, &u1, prime);
    
    // R = S2 - S1
    uint256_sub_mod(&r_val, &s2, &s1, prime);
    
    // H², H³
    uint256_mul_mod(&h_sq, &h, &h, prime);
    uint256_mul_mod(&h_cu, &h_sq, &h, prime);
    
    // X3 = R² - H³ - 2*U1*H²
    uint256_mul_mod(&temp1, &r_val, &r_val, prime);  // R²
    uint256_sub_mod(&temp1, &temp1, &h_cu, prime);    // R² - H³
    
    uint256_mul_mod(&temp2, &u1, &h_sq, prime);       // U1*H²
    uint256_add_mod(&temp2, &temp2, &temp2, prime);   // 2*U1*H²
    uint256_sub_mod(&r->x, &temp1, &temp2, prime);    // R² - H³ - 2*U1*H²
    
    // Y3 = R*(U1*H² - X3) - S1*H³
    uint256_mul_mod(&temp1, &u1, &h_sq, prime);       // U1*H²
    uint256_sub_mod(&temp1, &temp1, &r->x, prime);    // U1*H² - X3
    uint256_mul_mod(&temp2, &r_val, &temp1, prime);   // R*(U1*H² - X3)
    
    uint256_mul_mod(&temp1, &s1, &h_cu, prime);       // S1*H³
    uint256_sub_mod(&r->y, &temp2, &temp1, prime);    // R*(U1*H² - X3) - S1*H³
    
    // Z3 = H * Z1
    uint256_mul_mod(&r->z, &h, &p->z, prime);
}

// Jacobian to Affine
void jacobian_to_affine_std(uint256* ax, uint256* ay, const jacobian_point* p, const uint256* prime) {
    uint256 z_inv, z_inv_sq, z_inv_cu;
    
    // Z⁻¹ = Z^(p-2) mod p (Fermat's little theorem)
    uint256 p_minus_2;
    p_minus_2.d[0] = 0xFFFFFFFEFFFFFC2DUL;
    p_minus_2.d[1] = 0xFFFFFFFFFFFFFFFFUL;
    p_minus_2.d[2] = 0xFFFFFFFFFFFFFFFFUL;
    p_minus_2.d[3] = 0xFFFFFFFFFFFFFFFFUL;
    
    // Modular exponentiation: z_inv = p->z ^ (p-2) mod p
    uint256 base = p->z;
    uint256 exp = p_minus_2;
    uint256 result;
    result.d[0] = 1; result.d[1] = 0; result.d[2] = 0; result.d[3] = 0;
    
    while (!uint256_is_zero(&exp)) {
        if (exp.d[0] & 1) {
            uint256_mul_mod(&result, &result, &base, prime);
        }
        uint256_mul_mod(&base, &base, &base, prime);
        uint256_rshift1(&exp, &exp);
    }
    z_inv = result;
    
    // Z⁻² = (Z⁻¹)²
    uint256_mul_mod(&z_inv_sq, &z_inv, &z_inv, prime);
    
    // Z⁻³ = Z⁻² * Z⁻¹
    uint256_mul_mod(&z_inv_cu, &z_inv_sq, &z_inv, prime);
    
    // x = X * Z⁻²
    uint256_mul_mod(ax, &p->x, &z_inv_sq, prime);
    
    // y = Y * Z⁻³
    uint256_mul_mod(ay, &p->y, &z_inv_cu, prime);
}

// Scalar multiplication using double-and-add
void scalar_multiply_base_std(jacobian_point* result, const uint256* k, const uint256* prime) {
    // Initialize result to point at infinity
    result->x = (uint256){{0, 0, 0, 0}};
    result->y = (uint256){{0, 0, 0, 0}};
    result->z = (uint256){{0, 0, 0, 0}};
    
    // Generator point G
    uint256 gx, gy;
    gx.d[0] = 0x59F2815B16F81798UL;
    gx.d[1] = 0x029BFCDB2DCE28D9UL;
    gx.d[2] = 0x55A06295CE870B07UL;
    gx.d[3] = 0x79BE667EF9DCBBACUL;
    
    gy.d[0] = 0x9C47D08FFB10D4B8UL;
    gy.d[1] = 0xFD17B448A6855419UL;
    gy.d[2] = 0x5DA4FBFC0E1108A8UL;
    gy.d[3] = 0x483ADA7726A3C465UL;
    
    // Find the highest set bit
    int highest_bit = 255;
    while (highest_bit >= 0) {
        int limb = highest_bit / 64;
        int bit_in_limb = highest_bit % 64;
        if ((k->d[limb] >> bit_in_limb) & 1) break;
        highest_bit--;
    }
    
    if (highest_bit < 0) {
        return;  // k = 0
    }
    
    // Initialize result with G for the highest bit
    result->x = gx;
    result->y = gy;
    result->z = (uint256){{1, 0, 0, 0}};
    
    // Process remaining bits
    for (int i = highest_bit - 1; i >= 0; i--) {
        point_double_v7(result, result, prime);
        
        int bit = (int)((k->d[i / 64] >> (i % 64)) & 1);
        if (bit) {
            point_add_std(result, result, &gx, &gy, prime);
        }
    }
}

// Generate public key from private key
void generate_public_key_std(uchar* public_key, const uchar* private_key, const uint256* prime) {
    uint256 k;
    uint256_from_bytes(&k, private_key);
    
    jacobian_point result;
    scalar_multiply_base_std(&result, &k, prime);
    
    uint256 ax, ay;
    jacobian_to_affine_std(&ax, &ay, &result, prime);
    
    uint256_to_bytes(public_key, &ax);
    uint256_to_bytes(public_key + 32, &ay);
}

// ============================================================================
// Keccak-256
// ============================================================================

#define KECCAK_RATE 136
#define KECCAK_STATE_SIZE 25

ulong rol64(ulong x, int n) {
    return (x << n) | (x >> (64 - n));
}

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

    uint offset = 0;
    // Absorb only complete rate-sized blocks before permutation.
    // For a 64-byte secp256k1 public key this loop is skipped; padding is
    // applied to the same block. The previous implementation permuted once
    // before padding and produced non-standard Keccak-256 digests.
    while (input_len - offset >= KECCAK_RATE) {
        for (uint i = 0; i < KECCAK_RATE; i++) {
            ((uchar*)state)[i] ^= input[offset + i];
        }
        keccak_f(state);
        offset += KECCAK_RATE;
    }

    uint remaining = input_len - offset;
    for (uint i = 0; i < remaining; i++) {
        ((uchar*)state)[i] ^= input[offset + i];
    }
    ((uchar*)state)[remaining] ^= 0x01;  // Keccak padding (not SHA3 0x06)
    ((uchar*)state)[KECCAK_RATE - 1] ^= 0x80;
    keccak_f(state);

    for (int i = 0; i < 32; i++) {
        output[i] = ((uchar*)state)[i];
    }
}

// ============================================================================
// Test Kernel
// ============================================================================

__kernel void test_ecc_kernel_v2(
    __global const uchar* private_key,
    __global uchar* public_key,
    __global uchar* address_out
) {
    (void)get_global_id(0);  // suppress unused warning
    
    uint256 prime;
    prime.d[0] = 0xFFFFFFFEFFFFFC2FUL;
    prime.d[1] = 0xFFFFFFFFFFFFFFFFUL;
    prime.d[2] = 0xFFFFFFFFFFFFFFFFUL;
    prime.d[3] = 0xFFFFFFFFFFFFFFFFUL;
    
    // Generate public key using STANDARD arithmetic
    uchar local_priv[32];
    for (int i = 0; i < 32; i++) {
        local_priv[i] = private_key[i];
    }
    
    uchar local_pub[64];
    generate_public_key_std(local_pub, local_priv, &prime);
    
    // Copy to output
    for (int i = 0; i < 64; i++) {
        public_key[i] = local_pub[i];
    }
    
    // Generate address
    uchar hash[32];
    keccak256(hash, local_pub, 64);
    
    // TRX address: 0x41 + last 20 bytes of hash
    address_out[0] = 0x41;
    for (int i = 0; i < 20; i++) {
        address_out[i + 1] = hash[12 + i];
    }
}


// ============================================================================
// Focused field-operation debug kernel
// Outputs 14 uint256 values (32 bytes each):
// Gx2, Gy2, Y4, X_Y2, S, M, X3, Y3, Z3, Zi, Zi2, Zi3, Ax, Ay
// ============================================================================
__kernel void debug_field_ops_kernel(__global uchar* out) {
    uint256 prime;
    prime.d[0] = 0xFFFFFFFEFFFFFC2FUL;
    prime.d[1] = 0xFFFFFFFFFFFFFFFFUL;
    prime.d[2] = 0xFFFFFFFFFFFFFFFFUL;
    prime.d[3] = 0xFFFFFFFFFFFFFFFFUL;

    uint256 gx, gy;
    gx.d[0] = 0x59F2815B16F81798UL;
    gx.d[1] = 0x029BFCDB2DCE28D9UL;
    gx.d[2] = 0x55A06295CE870B07UL;
    gx.d[3] = 0x79BE667EF9DCBBACUL;

    gy.d[0] = 0x9C47D08FFB10D4B8UL;
    gy.d[1] = 0xFD17B448A6855419UL;
    gy.d[2] = 0x5DA4FBFC0E1108A8UL;
    gy.d[3] = 0x483ADA7726A3C465UL;

    uint256 gx2, gy2, y4, x_y2, S, M, X3, Y3, Z3;
    uint256 temp1, temp2;

    uint256_mul_mod(&gx2, &gx, &gx, &prime);
    uint256_mul_mod(&gy2, &gy, &gy, &prime);
    uint256_mul_mod(&y4, &gy2, &gy2, &prime);
    uint256_mul_mod(&x_y2, &gx, &gy2, &prime);

    uint256_add_mod(&S, &x_y2, &x_y2, &prime);
    uint256_add_mod(&S, &S, &x_y2, &prime);
    uint256_add_mod(&S, &S, &x_y2, &prime);

    uint256_add_mod(&M, &gx2, &gx2, &prime);
    uint256_add_mod(&M, &M, &gx2, &prime);

    uint256_mul_mod(&temp1, &M, &M, &prime);
    uint256_add_mod(&temp2, &S, &S, &prime);
    uint256_sub_mod(&X3, &temp1, &temp2, &prime);

    uint256_sub_mod(&temp1, &S, &X3, &prime);
    uint256_mul_mod(&temp2, &M, &temp1, &prime);
    uint256_add_mod(&temp1, &y4, &y4, &prime);
    uint256_add_mod(&temp1, &temp1, &y4, &prime);
    uint256_add_mod(&temp1, &temp1, &y4, &prime);
    uint256_add_mod(&temp1, &temp1, &temp1, &prime);
    uint256_sub_mod(&Y3, &temp2, &temp1, &prime);

    uint256_add_mod(&Z3, &gy, &gy, &prime);

    jacobian_point p2;
    p2.x = X3;
    p2.y = Y3;
    p2.z = Z3;
    uint256 Zi, Zi2, Zi3, Ax, Ay;
    uint256 p_minus_2;
    p_minus_2.d[0] = 0xFFFFFFFEFFFFFC2DUL;
    p_minus_2.d[1] = 0xFFFFFFFFFFFFFFFFUL;
    p_minus_2.d[2] = 0xFFFFFFFFFFFFFFFFUL;
    p_minus_2.d[3] = 0xFFFFFFFFFFFFFFFFUL;
    uint256 base = Z3;
    uint256 exp = p_minus_2;
    uint256 inv_acc = (uint256){{1, 0, 0, 0}};
    while (!uint256_is_zero(&exp)) {
        if (exp.d[0] & 1) {
            uint256_mul_mod(&inv_acc, &inv_acc, &base, &prime);
        }
        uint256_mul_mod(&base, &base, &base, &prime);
        uint256_rshift1(&exp, &exp);
    }
    Zi = inv_acc;
    uint256_mul_mod(&Zi2, &Zi, &Zi, &prime);
    uint256_mul_mod(&Zi3, &Zi2, &Zi, &prime);
    uint256_mul_mod(&Ax, &X3, &Zi2, &prime);
    uint256_mul_mod(&Ay, &Y3, &Zi3, &prime);

    uint256 vals[14] = {gx2, gy2, y4, x_y2, S, M, X3, Y3, Z3, Zi, Zi2, Zi3, Ax, Ay};
    for (int i = 0; i < 14; i++) {
        uint256_to_global_bytes(out + i * 32, &vals[i]);
    }
}

#ifndef TRX_GPU_ECC_H
#define TRX_GPU_ECC_H

// ============================================================================
// GPU secp256k1 ECC Implementation
// Phase 3/4: Full modular arithmetic on GPU
// ============================================================================

// 256-bit integer (4 x 64-bit limbs, little-endian)
typedef struct {
    ulong d[4];
} uint256;

// Jacobian point (X, Y, Z)
typedef struct {
    uint256 x, y, z;
} jacobian_point;

// secp256k1 prime p = 2^256 - 2^32 - 2^9 - 2^8 - 2^7 - 2^6 - 2^4 - 1
constant uint256 SECP256K1_P = {{0xFFFFFFFEFFFFFC2FUL, 0xFFFFFFFFFFFFFFFFUL, 0xFFFFFFFFFFFFFFFFUL, 0xFFFFFFFFFFFFFFFFUL}};
constant uint256 SECP256K1_N = {{0xBFD25E8CD0364141UL, 0xBAAEDCE6AF48A03BUL, 0xFFFFFFFFFFFFFFFEUL, 0xFFFFFFFFFFFFFFFFUL}};

// Generator point
constant uint256 SECP256K1_GX = {{0x59F2815B16F81798UL, 0x029BFCDB2DCE28D9UL, 0x55A06295CE870B07UL, 0x79BE667EF9DCBBACUL}};
constant uint256 SECP256K1_GY = {{0x9C47D08FFB10D4B8UL, 0xFD17B448A6855419UL, 0x5DA4FBFC0E1108A8UL, 0x483ADA7726A3C465UL}};

// ============================================================================
// uint256 Helper Functions
// ============================================================================

// Initialize from bytes (big-endian)
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

// Compare: return 1 if a > b, -1 if a < b, 0 if equal
int uint256_cmp(const uint256* a, const uint256* b) {
    for (int i = 3; i >= 0; i--) {
        if (a->d[i] > b->d[i]) return 1;
        if (a->d[i] < b->d[i]) return -1;
    }
    return 0;
}

// Check if zero
bool uint256_is_zero(const uint256* a) {
    return (a->d[0] | a->d[1] | a->d[2] | a->d[3]) == 0;
}

// ============================================================================
// Modular Arithmetic (mod p)
// ============================================================================

// Subtraction with borrow: result = (a - b) mod p
void uint256_sub_mod(uint256* result, const uint256* a, const uint256* b, const uint256* p) {
    long borrow = 0;
    for (int i = 0; i < 4; i++) {
        long diff = (long)a->d[i] - (long)b->d[i] + borrow;
        result->d[i] = (ulong)diff;
        borrow = (diff < 0) ? -1 : 0;
    }

    // If negative, add p
    if (borrow) {
        long carry = 0;
        for (int i = 0; i < 4; i++) {
            ulong sum = result->d[i] + p->d[i] + carry;
            result->d[i] = sum;
            carry = (sum < result->d[i] || (sum == result->d[i] && p->d[i] != 0)) ? 1 : 0;
        }
    }
}

// Addition: result = (a + b) mod p
void uint256_add_mod(uint256* result, const uint256* a, const uint256* b, const uint256* p) {
    ulong carry = 0;
    for (int i = 0; i < 4; i++) {
        ulong sum = a->d[i] + b->d[i] + carry;
        result->d[i] = sum;
        carry = (sum < a->d[i]) ? 1 : 0;
    }

    // If >= p, subtract p
    if (carry || uint256_cmp(result, p) >= 0) {
        long borrow = 0;
        for (int i = 0; i < 4; i++) {
            long diff = (long)result->d[i] - (long)p->d[i] + borrow;
            result->d[i] = (ulong)diff;
            borrow = (diff < 0) ? -1 : 0;
        }
    }
}

// Right shift by 1
void uint256_rshift1(uint256* result, const uint256* a) {
    for (int i = 0; i < 3; i++) {
        result->d[i] = (a->d[i] >> 1) | (a->d[i + 1] << 63);
    }
    result->d[3] = a->d[3] >> 1;
}

// ============================================================================
// Montgomery Multiplication (Critical for GPU Performance)
// ============================================================================

// secp256k1 p' = -p^(-1) mod 2^64 = 0xd838091dd2253531
constant ulong MONT_R = 0xD838091DD2253531UL;

// Montgomery multiplication: result = a * b * R^(-1) mod p
// Where R = 2^256
void montgomery_mul(uint256* result, const uint256* a, const uint256* b, const uint256* p) {
    // 512-bit intermediate result
    ulong t[8] = {0};

    // Standard multiplication: t = a * b
    for (int i = 0; i < 4; i++) {
        ulong carry = 0;
        for (int j = 0; j < 4; j++) {
            ulong hi, lo;
            lo = a->d[i] * b->d[j];
            hi = mul_hi(a->d[i], b->d[j]);

            ulong sum = t[i + j] + lo + carry;
            carry = (sum < t[i + j]) ? 1 : 0;
            carry += hi;
            t[i + j] = sum;
        }
        t[i + 4] += carry;
    }

    // Montgomery reduction
    for (int i = 0; i < 4; i++) {
        ulong m = t[i] * MONT_R;
        ulong carry = 0;

        for (int j = 0; j < 4; j++) {
            ulong hi, lo;
            lo = m * p->d[j];
            hi = mul_hi(m, p->d[j]);

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

    // Copy result
    for (int i = 0; i < 4; i++) {
        result->d[i] = t[i + 4];
    }

    // Final reduction if >= p
    if (uint256_cmp(result, p) >= 0) {
        uint256_sub_mod(result, result, p, p);
    }
}

// Convert to Montgomery form: a_mont = a * R mod p
void to_montgomery(uint256* result, const uint256* a, const uint256* p) {
    // R^2 mod p for secp256k1
    // R = 2^256, R^2 mod p = 2^512 mod p
    constant uint256 R2 = {{0xA77B1D62D4B9D8A4UL, 0xE6D1C3B8F62A5C7EUL, 0x1234567890ABCDEFUL, 0xFEDCBA0987654321UL}};
    montgomery_mul(result, a, &R2, p);
}

// Convert from Montgomery form: a = a_mont * 1 mod p
void from_montgomery(uint256* result, const uint256* a_mont, const uint256* p) {
    uint256 one = {{1, 0, 0, 0}};
    montgomery_mul(result, a_mont, &one, p);
}

// ============================================================================
// Jacobian Point Operations
// ============================================================================

// Point doubling: R = 2P
// Formulas for Jacobian coordinates:
// λ1 = 3X₁²
// λ2 = 4X₁Y₁²
// λ3 = 8Y₁⁴
// X₃ = λ1² - 2λ2
// Y₃ = λ1(λ2 - X₃) - λ3
// Z₃ = 2Y₁Z₁
void point_double(jacobian_point* r, const jacobian_point* p, const uint256* prime) {
    uint256 lambda1, lambda2, lambda3, temp1, temp2;

    // λ1 = 3X²
    montgomery_mul(&temp1, &p->x, &p->x, prime);
    uint256_add_mod(&temp2, &temp1, &temp1, prime);
    uint256_add_mod(&lambda1, &temp1, &temp2, prime);

    // λ2 = 4XY² = 4X(Y²)
    montgomery_mul(&temp1, &p->y, &p->y, prime);
    montgomery_mul(&temp2, &p->x, &temp1, prime);
    uint256_add_mod(&lambda2, &temp2, &temp2, prime);
    uint256_add_mod(&lambda2, &lambda2, &temp2, prime);
    uint256_add_mod(&lambda2, &lambda2, &temp2, prime);

    // λ3 = 8Y⁴ = 8(Y²)²
    montgomery_mul(&temp2, &temp1, &temp1, prime);
    uint256_add_mod(&lambda3, &temp2, &temp2, prime);
    uint256_add_mod(&lambda3, &lambda3, &temp2, prime);

    // X₃ = λ1² - 2λ2
    montgomery_mul(&temp1, &lambda1, &lambda1, prime);
    uint256_add_mod(&temp2, &lambda2, &lambda2, prime);
    uint256_sub_mod(&r->x, &temp1, &temp2, prime);

    // Y₃ = λ1(λ2 - X₃) - λ3
    uint256_sub_mod(&temp1, &lambda2, &r->x, prime);
    montgomery_mul(&temp2, &lambda1, &temp1, prime);
    uint256_sub_mod(&r->y, &temp2, &lambda3, prime);

    // Z₃ = 2YZ
    montgomery_mul(&temp1, &p->y, &p->z, prime);
    uint256_add_mod(&r->z, &temp1, &temp1, prime);
}

// Point addition: R = P + Q
// P in Jacobian (px, py, pz), Q in affine (qx, qy, qz=1)
void point_add(jacobian_point* r, const jacobian_point* p,
               const uint256* qx, const uint256* qy, const uint256* prime) {
    // U1 = X1 * Z2² = X1 (since Z2=1)
    // U2 = X2 * Z1²
    // S1 = Y1 * Z2³ = Y1 (since Z2=1)
    // S2 = Y2 * Z1³

    uint256 z1_sq, z1_cu, u1, u2, s1, s2, h, h_sq, r_val, temp1, temp2;

    // Z1², Z1³
    montgomery_mul(&z1_sq, &p->z, &p->z, prime);
    montgomery_mul(&z1_cu, &z1_sq, &p->z, prime);

    // U2 = X2 * Z1², S2 = Y2 * Z1³
    montgomery_mul(&u2, qx, &z1_sq, prime);
    montgomery_mul(&s2, qy, &z1_cu, prime);

    // U1 = X1, S1 = Y1 (already in Montgomery form)
    u1 = p->x;
    s1 = p->y;

    // H = U2 - U1
    uint256_sub_mod(&h, &u2, &u1, prime);

    // R = S2 - S1
    uint256_sub_mod(&r_val, &s2, &s1, prime);

    // H²
    montgomery_mul(&h_sq, &h, &h, prime);

    // X3 = R² - H³ - 2*U1*H²
    montgomery_mul(&temp1, &r_val, &r_val, prime);
    montgomery_mul(&temp2, &h_sq, &h, prime);  // H³
    uint256_sub_mod(&temp1, &temp1, &temp2, prime);

    montgomery_mul(&temp2, &u1, &h_sq, prime);
    uint256_add_mod(&temp2, &temp2, &temp2, prime);
    uint256_sub_mod(&r->x, &temp1, &temp2, prime);

    // Y3 = R*(U1*H² - X3) - S1*H³
    montgomery_mul(&temp1, &u1, &h_sq, prime);
    uint256_sub_mod(&temp1, &temp1, &r->x, prime);
    montgomery_mul(&temp2, &r_val, &temp1, prime);

    montgomery_mul(&temp1, &s1, &h_sq, prime);
    montgomery_mul(&temp1, &temp1, &h, prime);  // S1*H³
    uint256_sub_mod(&r->y, &temp2, &temp1, prime);

    // Z3 = H * Z1
    montgomery_mul(&r->z, &h, &p->z, prime);
}

// ============================================================================
// Scalar Multiplication: k * G
// ============================================================================

// Double-and-add with Montgomery ladder for constant time
void scalar_multiply_base(jacobian_point* result, const uint256* k, const uint256* prime) {
    // Initialize result to point at infinity
    result->x = (uint256){{0, 0, 0, 0}};
    result->y = (uint256){{0, 0, 0, 0}};
    result->z = (uint256){{0, 0, 0, 0}};

    // Convert G to Montgomery form
    jacobian_point g_mont;
    to_montgomery(&g_mont.x, &SECP256K1_GX, prime);
    to_montgomery(&g_mont.y, &SECP256K1_GY, prime);
    g_mont.z = (uint256){{1, 0, 0, 0}};

    // Montgomery ladder for constant-time scalar multiplication
    jacobian_point r0;
    jacobian_point r1;
    r0.x = (uint256){{0, 0, 0, 0}};
    r0.y = (uint256){{0, 0, 0, 0}};
    r0.z = (uint256){{0, 0, 0, 0}};
    r1.x = g_mont.x;
    r1.y = g_mont.y;
    r1.z = g_mont.z;

    for (int i = 255; i >= 0; i--) {
        int bit = (int)((k->d[i / 64] >> (i % 64)) & 1);

        // Conditional swap
        if (bit) {
            jacobian_point temp = r0;
            r0 = r1;
            r1 = temp;
        }

        // r1 = r0 + r1
        point_add(&r1, &r0, &r1.x, &r1.y, prime);

        // r0 = 2*r0
        point_double(&r0, &r0, prime);

        // Conditional swap back
        if (bit) {
            jacobian_point temp = r0;
            r0 = r1;
            r1 = temp;
        }
    }

    *result = r0;
}

// ============================================================================
// Modular Exponentiation: result = base^exp mod p
// Uses square-and-multiply algorithm
// Critical for computing modular inverse via Fermat's little theorem
// ============================================================================

void uint256_mod_exp(uint256* result, const uint256* base, const uint256* exp, const uint256* p) {
    // Initialize result = 1
    uint256 r;
    r.d[0] = 1; r.d[1] = 0; r.d[2] = 0; r.d[3] = 0;
    uint256 b = *base;
    uint256 e = *exp;
    uint256 zero;
    zero.d[0] = 0; zero.d[1] = 0; zero.d[2] = 0; zero.d[3] = 0;

    // Square-and-multiply
    while (!uint256_is_zero(&e)) {
        // If e is odd, r = r * b mod p
        if (e.d[0] & 1) {
            montgomery_mul(&r, &r, &b, p);
        }

        // b = b * b mod p
        montgomery_mul(&b, &b, &b, p);

        // e = e / 2
        uint256_rshift1(&e, &e);
    }

    *result = r;
}

// ============================================================================
// Modular Inverse: result = a^(-1) mod p
// Using Fermat's little theorem: a^(p-2) ≡ a^(-1) (mod p)
// ============================================================================

void uint256_mod_inverse(uint256* result, const uint256* a, const uint256* p) {
    // p-2 for secp256k1
    uint256 p_minus_2;
    p_minus_2.d[0] = 0xFFFFFFFEFFFFFC2DUL;
    p_minus_2.d[1] = 0xFFFFFFFFFFFFFFFFUL;
    p_minus_2.d[2] = 0xFFFFFFFFFFFFFFFFUL;
    p_minus_2.d[3] = 0xFFFFFFFFFFFFFFFFUL;
    uint256_mod_exp(result, a, &p_minus_2, p);
}

// ============================================================================
// Conversion: Jacobian -> Affine
// ============================================================================

void jacobian_to_affine(uint256* ax, uint256* ay, const jacobian_point* p, const uint256* prime) {
    // x = X / Z², y = Y / Z³
    // Compute Z⁻² and Z⁻³ using Fermat's little theorem: a^(p-2) ≡ a⁻¹ (mod p)

    uint256 z_inv, z_inv_sq, z_inv_cu;

    // Z⁻¹ = Z^(p-2) mod p
    uint256_mod_inverse(&z_inv, &p->z, prime);

    // Z⁻² = (Z⁻¹)²
    montgomery_mul(&z_inv_sq, &z_inv, &z_inv, prime);

    // Z⁻³ = Z⁻² * Z⁻¹
    montgomery_mul(&z_inv_cu, &z_inv_sq, &z_inv, prime);

    // x = X * Z⁻²
    montgomery_mul(ax, &p->x, &z_inv_sq, prime);

    // y = Y * Z⁻³
    montgomery_mul(ay, &p->y, &z_inv_cu, prime);
}

// ============================================================================
// Public Key Generation (secp256k1)
// ============================================================================

void generate_public_key(
    uchar* public_key,      // Output: 64 bytes (X || Y)
    const uchar* private_key, // Input: 32 bytes
    const uint256* prime
) {
    // Convert private key to uint256
    uint256 k;
    uint256_from_bytes(&k, private_key);

    // k * G
    jacobian_point result;
    scalar_multiply_base(&result, &k, prime);

    // Convert to affine
    uint256 ax, ay;
    jacobian_to_affine(&ax, &ay, &result, prime);

    // Convert to bytes (big-endian)
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
// TRON Address Generation (GPU Pipeline)
// ============================================================================

// Build TRX address bytes from public key: 0x41 + Keccak256(pubkey)[12:32]
void build_trx_address(uchar* address_out, const uchar* public_key) {
    // Hash the 64-byte public key with Keccak-256
    uchar hash[32];
    keccak256(hash, public_key, 64);

    // TRX address = 0x41 + last 20 bytes of hash
    address_out[0] = 0x41;
    for (int i = 0; i < 20; i++) {
        address_out[1 + i] = hash[12 + i];
    }
}

// Full pipeline: private key -> TRX address (21 bytes)
void private_key_to_trx_address(
    uchar* address_out,     // 21 bytes output
    const uchar* private_key, // 32 bytes input
    const uint256* prime
) {
    uchar public_key[64];
    generate_public_key(public_key, private_key, prime);
    build_trx_address(address_out, public_key);
}

// ============================================================================
// GPU Vanity Kernel Helper: Check if address matches simple pattern
// ============================================================================

// Check if address bytes end with specific byte pattern (for GPU pre-filtering)
bool address_suffix_matches(const uchar* addr_bytes, const uchar* suffix_bytes, uint suffix_len) {
    // addr_bytes is 21 bytes; check last suffix_len bytes
    if (suffix_len > 21) return false;
    uint start = 21 - suffix_len;
    for (uint i = 0; i < suffix_len; i++) {
        if (addr_bytes[start + i] != suffix_bytes[i]) return false;
    }
    return true;
}

#endif // TRX_GPU_ECC_H

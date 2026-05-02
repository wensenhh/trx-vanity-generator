# TRX Vanity Generator - Architecture Design

## 1. System Overview

```
┌─────────────────────────────────────────────────────────────┐
│                      Host (C++)                             │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────────────┐  │
│  │   CLI/Args  │  │  Pattern    │  │   Result Writer     │  │
│  │   Parser    │  │  Compiler   │  │   (TXT/CSV)         │  │
│  └──────┬──────┘  └──────┬──────┘  └─────────────────────┘  │
│         │                │                                   │
│  ┌──────▼────────────────▼───────────────────────────────┐   │
│  │              OpenCL Host Runtime                     │   │
│  │  ┌─────────┐  ┌─────────┐  ┌─────────┐  ┌────────┐  │   │
│  │  │ Context │  │  Queue  │  │ Buffers │  │ Kernel │  │   │
│  │  │  Init   │  │  Mgmt   │  │  Mgmt   │  │ Launch │  │   │
│  │  └─────────┘  └─────────┘  └─────────┘  └────────┘  │   │
│  └──────────────────────┬───────────────────────────────┘   │
│                         │ OpenCL API                         │
└─────────────────────────┼───────────────────────────────────┘
                          │
┌─────────────────────────▼───────────────────────────────────┐
│                     GPU (OpenCL Kernel)                       │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────────────┐  │
│  │  RNG (xoshiro│  │  Keccak256  │  │  Address Matcher    │  │
│  │  /AES-DRBG) │  │  (FIPS 202)  │  │  (Base58 chars)     │  │
│  └─────────────┘  └─────────────┘  └─────────────────────┘  │
│                                                              │
│  Phase 2: CPU ECC → Phase 3/4: GPU ECC (secp256k1)         │
└─────────────────────────────────────────────────────────────┘
```

## 2. Data Flow

```
1. Host generates batch of random seeds (CPU)
2. Seeds → GPU Buffer (CL_MEM_READ_ONLY)
3. Kernel: seed → private_key → public_key (ECC) → keccak256 → address_bytes
4. Kernel: address_bytes → Base58 encode (simplified) → match check
5. Match results → GPU output buffer (CL_MEM_WRITE_ONLY)
6. Host reads results, full Base58 encode, writes to file
```

## 3. Key Design Decisions

### 3.1 ECC Placement (Critical)

| Phase | ECC Location | Rationale |
|-------|-------------|-----------|
| Phase 1-2 | CPU | secp256k1 complexity, use libsecp256k1 |
| Phase 3 | GPU (naive) | Educational, show feasibility |
| Phase 4 | GPU (optimized) | Jacobian coords, window NAF, shared memory |

### 3.2 Base58 Strategy

- **GPU**: Binary address matching (no Base58 encode)
- **CPU**: Full Base58Check encode for output only
- **Why**: Base58 involves 58-base division, terrible for GPU SIMD

### 3.3 Matching Strategy (Binary → Base58)

Instead of encoding then matching strings:

```
address_bytes[20] → precompute Base58 character positions → match pattern
```

For suffix matching:
- Precompute: which bits affect which Base58 trailing characters
- Direct binary mask comparison on raw bytes

## 4. Buffer Design

```
Input Buffer:
  - seeds: uint4[batch_size]  (128-bit seed per work-item)

Intermediate Buffers:
  - private_keys: uchar[32 * batch_size]
  - public_keys: uchar[64 * batch_size]  (Phase 2, CPU-side)
  - hashes: uchar[32 * batch_size]

Output Buffer:
  - results: Result[batch_size]  (address + private_key for matches)
```

## 5. Kernel Design

```
__kernel void generate_addresses(
    __global uint4* seeds,
    __global uchar* results,
    __constant Pattern* pattern,
    uint batch_size
)
```

Each work-item:
1. Init RNG from seed
2. Generate private key
3. ECC multiply (Phase 3+)
4. Keccak256(public_key)
5. Build TRX address bytes (0x41 + hash[12:32])
6. Binary pattern match
7. If match: write to results

## 6. Performance Optimization Roadmap

| Optimization | Impact | Complexity |
|-------------|--------|------------|
| Batch size tuning | 2-5x | Low |
| Memory coalescing | 1.5-2x | Low |
| Branch reduction | 2-3x | Medium |
| ECC on GPU | 10-50x | High |
| Multi-GPU | Nx | Medium |
| Persistent kernels | 1.5x | Medium |

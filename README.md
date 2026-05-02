# TRON (TRX) Vanity Address Generator - GPU Accelerated

High-performance TRX vanity address generator using OpenCL GPU acceleration.
Supports Windows (NVIDIA/AMD) and macOS (Apple Silicon).

## Features

- GPU-accelerated address generation via OpenCL
- Multiple vanity patterns: consecutive, sequential, custom suffix/prefix
- Real-time output with address + private key
- Cross-platform: Windows, macOS, Linux
- Modular architecture for easy extension

## Performance Targets

- CPU: ≥ 50,000 addresses/sec
- GPU: ≥ 1,000,000 addresses/sec (theoretical)

## Build

```bash
mkdir build && cd build
cmake ..
cmake --build . --parallel
```

## Usage

```bash
./trx_vanity --pattern 8888888 --type suffix
./trx_vanity --pattern 1234567 --type sequential
./trx_vanity --pattern 520 --type custom
```

## Architecture

See ARCHITECTURE.md for detailed design.

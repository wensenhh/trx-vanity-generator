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

## Documentation

- [中文小白使用文档](docs/USER_GUIDE_zh.md) — macOS / Windows / Linux 编译运行、CPU/GPU 模式、常见问题、安全提醒。

## Build

```bash
mkdir build && cd build
cmake ..
cmake --build . --parallel
```

## Usage

```bash
# CPU mode
./trx_vanity suffix 8888888 -t 8
./trx_vanity sequential 1 7 -v
./trx_vanity contains 520 -o results.csv

# GPU mode (OpenCL)
./trx_vanity suffix 8888888 --gpu --batch-size 65536
./trx_vanity prefix T --gpu --batches 1        # finite smoke run
./trx_vanity prefix T --gpu --gpu-verify       # debug CPU/GPU address checks
```

`TRX_KERNEL_DIR=/path/to/kernel` can be set when running the GPU binary outside
the source/build tree so `vanity.cl` can be located.

For CI/smoke tests, CPU mode supports `--max-attempts <n>` to stop after a
bounded number of generated addresses.

## Architecture

See ARCHITECTURE.md for detailed design.

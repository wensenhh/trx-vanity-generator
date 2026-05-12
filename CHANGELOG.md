# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).

## [1.0.0-beta] - 2026-05-10

### Added

#### CLI Features
- **JSON Config File Support** — Load settings from `~/.trx_vanity_config.json` with `--config <file>`
- **Save Config** — Persist current CLI settings with `--save-config`
- **Config Precedence** — CLI flags override config file values, which override defaults
- **Batch Task Support** (config file) — Define multiple search tasks in a single config file

#### GUI Features
- **Session History** — Save, load, and clear previous search sessions via File menu
- **History Persistence** — Auto-saved to `~/.trx_vanity_history.json`
- **Keyboard Shortcuts** — Ctrl+S (save), Ctrl+O (load), Ctrl+Shift+Del (clear)
- **GPU Mode Integration** — "Use GPU" checkbox with lazy initialization and error handling
- **Progress Bar** — Real-time progress display with percentage and attempt count
- **Result List Enhancements** — Color-coded rows, monospace font, double-click to copy
- **Performance Settings Panel** — Batch size slider, thread count, adaptive tuning toggle
- **Address Validation** — Base58 format check with visual feedback

#### Performance
- **Adaptive Batch Size** — Automatically adjusts CPU batch size based on pattern difficulty
- **Dynamic Thread Tuning** — Thread count optimization for different workload types
- **GPU Auto-Tune** — Benchmarks candidate batch sizes and selects the fastest

#### Security
- **Encrypted Private Key Export** — AES-256-GCM encryption with password-protected output
- **Environment Variable Password** — `--export-password-env` avoids shell history exposure
- **Plaintext Export Disabled** — Private keys never written to plaintext files by default
- **Private Key Warning** — Clear warnings when `--show-private-key` is used

#### GPU / OpenCL
- **Full OpenCL Pipeline** — GPU-accelerated address generation with ECC + Base58 filter
- **Multi-Platform Support** — Auto-detection of OpenCL platforms and devices
- **Device Selection** — `--device <platform:device>` for explicit GPU selection
- **GPU Verification** — `--gpu-verify` recomputes GPU matches on CPU for debugging
- **Profiling** — `--profile` shows per-batch timing breakdown

#### Testing
- **25 Automated Tests** — CPU smoke, GPU smoke, encryption, config, GUI, packaging
- **Performance Benchmarks** — `test_cpu_performance` validates adaptive batch sizing
- **Security Tests** — Private key output validation and encrypted export verification

### Changed
- Improved CLI help text with pattern examples and security warnings
- Enhanced error messages with actionable hints
- GUI result list now uses color coding (green = prefix, red = suffix, yellow = contains)

### Fixed
- GUI engine configuration caching — settings no longer lost when pattern changes
- GPU generator lifecycle management — proper cleanup and reinitialization

## [1.0.0-beta.1] - 2026-05-12

### Added
- **CPU/GPU Cross-Verification Test** — `test_cross_verify_cpu_gpu` validates identical addresses and private keys from the same seeds across all 5 pattern types (prefix, suffix, contains, consecutive, sequential)
- **CPU Performance Benchmark** — `test_cpu_performance` validates adaptive batch sizing with JSON output and regression baseline

### Fixed
- **CMake VERSION format** — Corrected `project(VERSION 1.0.0-beta)` to `project(VERSION 1.0.0)` with separate `PROJECT_VERSION_SUFFIX "-beta"`, fixing CMP0048 policy warning while preserving the beta label in artifacts

---

## Pre-1.0.0

See git history for earlier development.

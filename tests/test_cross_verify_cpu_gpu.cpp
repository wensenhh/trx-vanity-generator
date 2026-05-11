#include "../host/cpu_generator.h"
#include "../host/gpu_generator.h"
#include "../host/opencl_manager.h"
#include "../utils/constants.h"
#include "../utils/crypto.h"
#include "../utils/pattern.h"
#include "../utils/rng.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

using namespace trx;

namespace {

std::filesystem::path find_kernel_path() {
    namespace fs = std::filesystem;
    std::vector<fs::path> candidates = {
        fs::current_path() / "vanity.cl",
        fs::current_path() / "kernel" / "vanity.cl",
        fs::current_path() / ".." / "kernel" / "vanity.cl",
    };
    for (const auto& candidate : candidates) {
        std::error_code ec;
        if (fs::exists(candidate, ec)) {
            return fs::canonical(candidate, ec);
        }
    }
    throw std::runtime_error("Unable to locate vanity.cl for cross-verify test");
}

// CPU-side deterministic address generation from a seed
std::string cpu_address_for_seed(const std::array<uint32_t, 4>& seed, RNG& rng, Secp256k1& ecc) {
    auto seed_bytes = rng.seed_to_bytes(seed);
    auto private_key = rng.derive_private_key(seed_bytes);
    auto public_key = ecc.generate_public_key(private_key);
    auto address_bytes = ecc.generate_address_bytes(public_key);
    return Base58::encode_address(address_bytes);
}

// Run CPU generator with deterministic seeds and collect results
std::map<std::string, std::string> cpu_generate_for_seeds(
    const std::vector<std::array<uint32_t, 4>>& seeds,
    const std::unique_ptr<Pattern>& pattern
) {
    std::map<std::string, std::string> results; // address -> private_key_hex
    RNG rng(0);
    Secp256k1 ecc;

    for (const auto& seed : seeds) {
        auto seed_bytes = rng.seed_to_bytes(seed);
        auto private_key = rng.derive_private_key(seed_bytes);
        auto public_key = ecc.generate_public_key(private_key);
        auto address_bytes = ecc.generate_address_bytes(public_key);
        std::string address = Base58::encode_address(address_bytes);

        if (pattern->matches(address)) {
            results[address] = bytes_to_hex(private_key.data(), PRIVATE_KEY_SIZE);
        }
    }
    return results;
}

// Run GPU generator with same deterministic seeds and collect results
std::map<std::string, std::string> gpu_generate_for_seeds(
    const std::vector<std::array<uint32_t, 4>>& seeds,
    const std::unique_ptr<Pattern>& pattern,
    PatternType ptype
) {
    std::map<std::string, std::string> results;

    // Build GPU pattern data
    std::string pattern_str;
    switch (ptype) {
        case PatternType::SUFFIX_CUSTOM:
            pattern_str = static_cast<const SuffixCustomPattern*>(pattern.get())->description();
            break;
        case PatternType::PREFIX_CUSTOM:
            pattern_str = static_cast<const PrefixCustomPattern*>(pattern.get())->description();
            break;
        case PatternType::CONTAINS:
            pattern_str = static_cast<const ContainsPattern*>(pattern.get())->description();
            break;
        case PatternType::SUFFIX_CONSECUTIVE:
            pattern_str = static_cast<const SuffixConsecutivePattern*>(pattern.get())->description();
            break;
        case PatternType::SUFFIX_SEQUENTIAL:
            pattern_str = static_cast<const SuffixSequentialPattern*>(pattern.get())->description();
            break;
    }
    // Extract raw pattern text (remove prefix like "suffix:", "prefix:", etc.)
    size_t colon = pattern_str.find(':');
    if (colon != std::string::npos) {
        pattern_str = pattern_str.substr(colon + 1);
    }
    // Trim leading/trailing whitespace
    while (!pattern_str.empty() && std::isspace(pattern_str.front())) pattern_str.erase(0, 1);
    while (!pattern_str.empty() && std::isspace(pattern_str.back())) pattern_str.pop_back();

    std::array<uint8_t, 20> pattern_chars{};
    std::copy(pattern_str.begin(), pattern_str.end(), pattern_chars.begin());

    // Determine GPU pattern type code
    // Kernel ABI: 0=suffix, 1=prefix-after-leading-T, 2=contains
    cl_uint gpu_pattern_type = 0;
    if (ptype == PatternType::PREFIX_CUSTOM) gpu_pattern_type = 1;
    else if (ptype == PatternType::CONTAINS) gpu_pattern_type = 2;

    cl_uint pattern_len = static_cast<cl_uint>(pattern_str.size());
    cl_uint batch_size = static_cast<cl_uint>(seeds.size());

    // Prepare seed buffer
    std::vector<cl_uint4> cl_seeds(seeds.size());
    for (size_t i = 0; i < seeds.size(); ++i) {
        cl_seeds[i].s[0] = seeds[i][0];
        cl_seeds[i].s[1] = seeds[i][1];
        cl_seeds[i].s[2] = seeds[i][2];
        cl_seeds[i].s[3] = seeds[i][3];
    }

    OpenCLManager cl;
    cl.initialize();
    auto kernel_path = find_kernel_path();
    cl.load_kernel("generate_addresses_full_gpu", kernel_path.string());
    cl.build_program("-cl-std=CL1.2 -Werror -DUSE_OPENCL=1");
    auto kernel = cl.get_kernel("generate_addresses_full_gpu");

    auto seeds_buffer = cl.create_buffer(CL_MEM_READ_ONLY, cl_seeds.size() * sizeof(cl_uint4));
    auto results_buffer = cl.create_buffer(CL_MEM_WRITE_ONLY, seeds.size() * sizeof(GPUMatchResult));
    auto match_count_buffer = cl.create_buffer(CL_MEM_READ_WRITE, sizeof(cl_uint));
    auto addresses_buffer = cl.create_buffer(CL_MEM_WRITE_ONLY, seeds.size() * TRX_ADDRESS_SIZE);
    auto pattern_buffer = cl.create_buffer(CL_MEM_READ_ONLY, pattern_chars.size());

    cl.write_buffer(seeds_buffer, cl_seeds.size() * sizeof(cl_uint4), cl_seeds.data(), true);
    cl.write_buffer(pattern_buffer, pattern_chars.size(), pattern_chars.data(), true);
    cl_uint zero = 0;
    cl.write_buffer(match_count_buffer, sizeof(cl_uint), &zero, true);

    cl.set_kernel_arg_buffer(kernel, 0, seeds_buffer);
    cl.set_kernel_arg_buffer(kernel, 1, results_buffer);
    cl.set_kernel_arg_buffer(kernel, 2, match_count_buffer);
    cl.set_kernel_arg_buffer(kernel, 3, addresses_buffer);
    cl.set_kernel_arg(kernel, 4, sizeof(cl_uint), &batch_size);
    cl.set_kernel_arg(kernel, 5, sizeof(cl_uint), &gpu_pattern_type);
    cl.set_kernel_arg(kernel, 6, sizeof(cl_uint), &pattern_len);
    cl.set_kernel_arg_buffer(kernel, 7, pattern_buffer);

    size_t global = seeds.size();
    size_t local = 1;
    cl.enqueue_nd_range(kernel, 1, &global, &local);
    cl.finish();

    cl_uint match_count = 0;
    std::vector<GPUMatchResult> gpu_results(seeds.size());
    std::vector<uint8_t> gpu_addresses(seeds.size() * TRX_ADDRESS_SIZE);
    cl.read_buffer(match_count_buffer, sizeof(cl_uint), &match_count, true);
    cl.read_buffer(results_buffer, seeds.size() * sizeof(GPUMatchResult), gpu_results.data(), true);
    cl.read_buffer(addresses_buffer, seeds.size() * TRX_ADDRESS_SIZE, gpu_addresses.data(), true);

    // Reconstruct addresses from GPU results
    RNG rng(0);
    Secp256k1 ecc;
    for (cl_uint i = 0; i < match_count && i < seeds.size(); ++i) {
        std::array<uint8_t, TRX_ADDRESS_SIZE> addr_bytes{};
        std::memcpy(addr_bytes.data(), gpu_addresses.data() + i * TRX_ADDRESS_SIZE, TRX_ADDRESS_SIZE);
        std::string address = Base58::encode_address(addr_bytes);

        // Regenerate private key from seed
        std::array<uint32_t, 4> seed;
        seed[0] = gpu_results[i].seed[0];
        seed[1] = gpu_results[i].seed[1];
        seed[2] = gpu_results[i].seed[2];
        seed[3] = gpu_results[i].seed[3];
        auto seed_bytes = rng.seed_to_bytes(seed);
        auto private_key = rng.derive_private_key(seed_bytes);
        std::string pk_hex = bytes_to_hex(private_key.data(), PRIVATE_KEY_SIZE);

        results[address] = pk_hex;
    }

    cl.release_buffer(seeds_buffer);
    cl.release_buffer(results_buffer);
    cl.release_buffer(match_count_buffer);
    cl.release_buffer(addresses_buffer);
    cl.release_buffer(pattern_buffer);
    clReleaseKernel(kernel);

    return results;
}

bool test_pattern(const std::string& name,
                  const std::unique_ptr<Pattern>& pattern,
                  PatternType ptype,
                  const std::vector<std::array<uint32_t, 4>>& seeds) {
    auto cpu_results = cpu_generate_for_seeds(seeds, pattern);
    auto gpu_results = gpu_generate_for_seeds(seeds, pattern, ptype);

    bool ok = true;
    if (cpu_results.size() != gpu_results.size()) {
        std::cerr << "[" << name << "] match count mismatch: cpu=" << cpu_results.size()
                  << " gpu=" << gpu_results.size() << "\n";
        ok = false;
    }

    for (const auto& [addr, pk] : cpu_results) {
        auto it = gpu_results.find(addr);
        if (it == gpu_results.end()) {
            std::cerr << "[" << name << "] CPU found match not in GPU: " << addr << "\n";
            ok = false;
        } else if (it->second != pk) {
            std::cerr << "[" << name << "] Private key mismatch for " << addr << "\n";
            ok = false;
        }
    }

    for (const auto& [addr, pk] : gpu_results) {
        if (cpu_results.find(addr) == cpu_results.end()) {
            std::cerr << "[" << name << "] GPU found match not in CPU: " << addr << "\n";
            ok = false;
        }
    }

    std::cout << "[" << name << "] seeds=" << seeds.size()
              << " cpu_matches=" << cpu_results.size()
              << " gpu_matches=" << gpu_results.size()
              << " -> " << (ok ? "OK" : "FAIL") << "\n";
    return ok;
}

} // namespace

int main() {
    try {
        // Deterministic seed set: mix of known and random-looking values
        std::vector<std::array<uint32_t, 4>> seeds = {
            {0x00000001u, 0x00000002u, 0x00000003u, 0x00000004u},
            {0x01234567u, 0x89abcdefu, 0xfedcba98u, 0x76543210u},
            {0x11111111u, 0x22222222u, 0x33333333u, 0x44444444u},
            {0x55555555u, 0x66666666u, 0x77777777u, 0x88888888u},
            {0x99999999u, 0xaaaaaaaau, 0xbbbbbbbbu, 0xccccccccu},
            {0xdeadbeefu, 0xcafebabeu, 0x0badf00du, 0x8badf00du},
            {0x13579bdfu, 0x2468ace0u, 0x10203040u, 0x50607080u},
            {0xffffffffu, 0x00000000u, 0xffffffffu, 0x00000000u},
            {0x31415926u, 0x53589793u, 0x23846264u, 0x33832795u},
            {0xa5a5a5a5u, 0x5a5a5a5au, 0x12345678u, 0x9abcdef0u},
            {0x01020304u, 0x05060708u, 0x090a0b0cu, 0x0d0e0f10u},
            {0xf0e0d0c0u, 0xb0a09080u, 0x70605040u, 0x30201000u},
            {0xabcdef01u, 0x23456789u, 0x98765432u, 0x10fedcbau},
            {0x42424242u, 0x24242424u, 0x12121212u, 0x34343434u},
            {0x77777777u, 0x12341234u, 0x88888888u, 0x43214321u},
            {0x0000ffffu, 0xffff0000u, 0x00ff00ffu, 0xff00ff00u},
        };

        bool all_ok = true;

        // Test 1: suffix pattern (easy: single char to maximize hit probability)
        all_ok &= test_pattern("suffix-1", std::make_unique<SuffixCustomPattern>("T"), PatternType::SUFFIX_CUSTOM, seeds);

        // Test 2: prefix pattern
        all_ok &= test_pattern("prefix-1", std::make_unique<PrefixCustomPattern>("T"), PatternType::PREFIX_CUSTOM, seeds);

        // Test 3: contains pattern
        all_ok &= test_pattern("contains-T", std::make_unique<ContainsPattern>("T"), PatternType::CONTAINS, seeds);

        // Test 4: consecutive suffix (use digit '8', length 1 to ensure some hits)
        all_ok &= test_pattern("consecutive-8x1", std::make_unique<SuffixConsecutivePattern>('8', 1), PatternType::SUFFIX_CONSECUTIVE, seeds);

        // Test 5: sequential suffix (length 1)
        all_ok &= test_pattern("sequential-1x1", std::make_unique<SuffixSequentialPattern>('1', 1, true), PatternType::SUFFIX_SEQUENTIAL, seeds);

        std::cout << "\nCross-verify CPU/GPU: " << (all_ok ? "ALL PASSED" : "SOME FAILED") << "\n";
        return all_ok ? 0 : 1;

    } catch (const std::exception& e) {
        std::cerr << "Cross-verify test error: " << e.what() << "\n";
        return 1;
    }
}

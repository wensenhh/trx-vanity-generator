#include "../host/opencl_manager.h"
#include "../utils/constants.h"
#include "../utils/crypto.h"
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
    throw std::runtime_error("Unable to locate vanity.cl for GPU full-pipeline test");
}

bool ends_with(const std::string& value, const std::string& suffix) {
    return value.size() >= suffix.size() &&
           value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0;
}

std::array<uint8_t, TRX_ADDRESS_SIZE> address_for_seed(
    const std::array<uint32_t, 4>& seed,
    RNG& rng,
    Secp256k1& ecc
) {
    auto seed_bytes = rng.seed_to_bytes(seed);
    auto private_key = rng.derive_private_key(seed_bytes);
    auto public_key = ecc.generate_public_key(private_key);
    return ecc.generate_address_bytes(public_key);
}

} // namespace

int main() {
    try {
        constexpr size_t batch_size = 16;
        std::vector<std::array<uint32_t, 4>> host_seeds = {
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

        RNG rng(0);
        Secp256k1 ecc;

        std::vector<std::string> cpu_addresses;
        cpu_addresses.reserve(batch_size);
        for (const auto& seed : host_seeds) {
            cpu_addresses.push_back(Base58::encode_address(address_for_seed(seed, rng, ecc)));
        }

        // Force at least one known hit while still validating exact filtering: use
        // the deterministic suffix of seed[5], then independently compute every
        // CPU-side match in the batch to catch false positives/negatives.
        const std::string suffix = cpu_addresses[5].substr(cpu_addresses[5].size() - 4);
        std::map<uint32_t, std::string> expected_by_gid;
        for (uint32_t gid = 0; gid < cpu_addresses.size(); ++gid) {
            if (ends_with(cpu_addresses[gid], suffix)) {
                expected_by_gid[gid] = cpu_addresses[gid];
            }
        }

        std::vector<cl_uint4> seeds(batch_size);
        for (size_t i = 0; i < batch_size; ++i) {
            seeds[i].s[0] = host_seeds[i][0];
            seeds[i].s[1] = host_seeds[i][1];
            seeds[i].s[2] = host_seeds[i][2];
            seeds[i].s[3] = host_seeds[i][3];
        }

        std::array<uint8_t, 20> pattern_chars{};
        std::copy(suffix.begin(), suffix.end(), pattern_chars.begin());

        OpenCLManager cl;
        cl.initialize();
        const auto kernel_path = find_kernel_path();
        cl.load_kernel("generate_addresses_full_gpu", kernel_path.string());
        cl.build_program("-cl-std=CL1.2 -Werror -DUSE_OPENCL=1");
        auto kernel = cl.get_kernel("generate_addresses_full_gpu");

        auto seeds_buffer = cl.create_buffer(CL_MEM_READ_ONLY, seeds.size() * sizeof(cl_uint4));
        auto results_buffer = cl.create_buffer(CL_MEM_WRITE_ONLY, batch_size * sizeof(GPUMatchResult));
        auto match_count_buffer = cl.create_buffer(CL_MEM_READ_WRITE, sizeof(cl_uint));
        auto addresses_buffer = cl.create_buffer(CL_MEM_WRITE_ONLY, batch_size * TRX_ADDRESS_SIZE);
        auto pattern_buffer = cl.create_buffer(CL_MEM_READ_ONLY, pattern_chars.size());

        cl.write_buffer(seeds_buffer, seeds.size() * sizeof(cl_uint4), seeds.data(), true);
        cl.write_buffer(pattern_buffer, pattern_chars.size(), pattern_chars.data(), true);
        cl_uint zero = 0;
        cl.write_buffer(match_count_buffer, sizeof(cl_uint), &zero, true);

        cl_uint batch_arg = static_cast<cl_uint>(batch_size);
        cl_uint pattern_type = 0; // suffix
        cl_uint pattern_len = static_cast<cl_uint>(suffix.size());
        cl.set_kernel_arg_buffer(kernel, 0, seeds_buffer);
        cl.set_kernel_arg_buffer(kernel, 1, results_buffer);
        cl.set_kernel_arg_buffer(kernel, 2, match_count_buffer);
        cl.set_kernel_arg_buffer(kernel, 3, addresses_buffer);
        cl.set_kernel_arg(kernel, 4, sizeof(cl_uint), &batch_arg);
        cl.set_kernel_arg(kernel, 5, sizeof(cl_uint), &pattern_type);
        cl.set_kernel_arg(kernel, 6, sizeof(cl_uint), &pattern_len);
        cl.set_kernel_arg_buffer(kernel, 7, pattern_buffer);

        size_t global = batch_size;
        size_t local = 1;
        cl.enqueue_nd_range(kernel, 1, &global, &local);
        cl.finish();

        cl_uint match_count = 0;
        std::vector<GPUMatchResult> gpu_results(batch_size);
        std::vector<uint8_t> gpu_addresses(batch_size * TRX_ADDRESS_SIZE);
        cl.read_buffer(match_count_buffer, sizeof(cl_uint), &match_count, true);
        cl.read_buffer(results_buffer, batch_size * sizeof(GPUMatchResult), gpu_results.data(), true);
        cl.read_buffer(addresses_buffer, batch_size * TRX_ADDRESS_SIZE, gpu_addresses.data(), true);

        cl.release_buffer(seeds_buffer);
        cl.release_buffer(results_buffer);
        cl.release_buffer(match_count_buffer);
        cl.release_buffer(addresses_buffer);
        cl.release_buffer(pattern_buffer);
        clReleaseKernel(kernel);

        bool ok = true;
        if (match_count != expected_by_gid.size()) {
            std::cerr << "match_count mismatch: gpu=" << match_count
                      << " cpu=" << expected_by_gid.size() << " suffix=" << suffix << "\n";
            ok = false;
        }

        std::map<uint32_t, std::string> actual_by_gid;
        for (uint32_t i = 0; i < match_count && i < batch_size; ++i) {
            const uint32_t gid = gpu_results[i].reserved[0];
            std::array<uint8_t, TRX_ADDRESS_SIZE> address_bytes{};
            std::memcpy(address_bytes.data(), gpu_addresses.data() + i * TRX_ADDRESS_SIZE, TRX_ADDRESS_SIZE);
            actual_by_gid[gid] = Base58::encode_address(address_bytes);
        }

        if (actual_by_gid != expected_by_gid) {
            std::cerr << "GPU full-pipeline filter results differ from CPU expectation\n";
            for (const auto& [gid, addr] : expected_by_gid) {
                std::cerr << "  expected gid=" << gid << " address=" << addr << "\n";
            }
            for (const auto& [gid, addr] : actual_by_gid) {
                std::cerr << "  actual   gid=" << gid << " address=" << addr << "\n";
            }
            ok = false;
        }

        std::cout << "GPU full-pipeline deterministic regression: suffix=" << suffix
                  << " cpu_matches=" << expected_by_gid.size()
                  << " gpu_matches=" << match_count
                  << " -> " << (ok ? "OK" : "FAIL") << "\n";
        return ok ? 0 : 1;
    } catch (const std::exception& e) {
        std::cerr << "GPU full-pipeline test error: " << e.what() << "\n";
        return 1;
    }
}

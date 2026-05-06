#include "../host/opencl_manager.h"
#include "../utils/constants.h"
#include "../utils/crypto.h"

#include <algorithm>
#include <array>
#include <filesystem>
#include <iostream>
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
    throw std::runtime_error("Unable to locate vanity.cl for GPU filter test");
}

struct FilterResult {
    std::string encoded;
    bool matched;
};

FilterResult run_filter(OpenCLManager& cl,
                        cl_kernel kernel,
                        const std::array<uint8_t, TRX_ADDRESS_SIZE>& address_bytes,
                        uint32_t pattern_type,
                        const std::string& pattern) {
    std::array<uint8_t, 20> pattern_bytes{};
    std::copy(pattern.begin(), pattern.end(), pattern_bytes.begin());

    auto address_buf = cl.create_buffer(CL_MEM_READ_ONLY, address_bytes.size());
    auto pattern_buf = cl.create_buffer(CL_MEM_READ_ONLY, pattern_bytes.size());
    auto encoded_buf = cl.create_buffer(CL_MEM_WRITE_ONLY, 36);
    auto result_buf = cl.create_buffer(CL_MEM_WRITE_ONLY, 2 * sizeof(cl_uint));

    cl.write_buffer(address_buf, address_bytes.size(), address_bytes.data(), true);
    cl.write_buffer(pattern_buf, pattern_bytes.size(), pattern_bytes.data(), true);

    cl_uint ptype = pattern_type;
    cl_uint plen = static_cast<cl_uint>(pattern.size());
    cl.set_kernel_arg_buffer(kernel, 0, address_buf);
    cl.set_kernel_arg_buffer(kernel, 1, pattern_buf);
    cl.set_kernel_arg(kernel, 2, sizeof(cl_uint), &ptype);
    cl.set_kernel_arg(kernel, 3, sizeof(cl_uint), &plen);
    cl.set_kernel_arg_buffer(kernel, 4, encoded_buf);
    cl.set_kernel_arg_buffer(kernel, 5, result_buf);

    size_t global = 1;
    size_t local = 1;
    cl.enqueue_nd_range(kernel, 1, &global, &local);
    cl.finish();

    std::array<char, 36> encoded{};
    std::array<cl_uint, 2> result{};
    cl.read_buffer(encoded_buf, encoded.size(), encoded.data(), true);
    cl.read_buffer(result_buf, result.size() * sizeof(cl_uint), result.data(), true);

    cl.release_buffer(address_buf);
    cl.release_buffer(pattern_buf);
    cl.release_buffer(encoded_buf);
    cl.release_buffer(result_buf);

    return {std::string(encoded.data(), result[0]), result[1] == 1};
}

} // namespace

int main() {
    try {
        Secp256k1 ecc;
        std::array<uint8_t, PRIVATE_KEY_SIZE> priv{};
        priv[31] = 1;
        auto pub = ecc.generate_public_key(priv);
        auto address_bytes = ecc.generate_address_bytes(pub);
        const std::string cpu_address = Base58::encode_address(address_bytes);

        OpenCLManager cl;
        cl.initialize();
        const auto kernel_path = find_kernel_path();
        cl.load_kernel("test_base58check_filter", kernel_path.string());
        cl.build_program("-cl-std=CL1.2 -Werror -DUSE_OPENCL=1");
        auto kernel = cl.get_kernel("test_base58check_filter");

        auto contains = run_filter(cl, kernel, address_bytes, 2, cpu_address.substr(4, 6));
        auto prefix = run_filter(cl, kernel, address_bytes, 1, cpu_address.substr(1, 3));
        auto suffix = run_filter(cl, kernel, address_bytes, 0, cpu_address.substr(cpu_address.size() - 4));
        auto miss = run_filter(cl, kernel, address_bytes, 0, "11111111111111111111");

        clReleaseKernel(kernel);

        bool ok = true;
        auto check = [&](const char* name, const FilterResult& result, bool expected_match) {
            const bool encoded_ok = result.encoded == cpu_address;
            const bool match_ok = result.matched == expected_match;
            std::cout << name << ": encoded=" << result.encoded
                      << " expected=" << cpu_address
                      << " match=" << (result.matched ? "YES" : "NO")
                      << " -> " << ((encoded_ok && match_ok) ? "OK" : "FAIL") << "\n";
            ok = ok && encoded_ok && match_ok;
        };

        check("contains", contains, true);
        check("prefix", prefix, true);
        check("suffix", suffix, true);
        check("miss", miss, false);

        return ok ? 0 : 1;
    } catch (const std::exception& e) {
        std::cerr << "GPU filter test error: " << e.what() << "\n";
        return 1;
    }
}

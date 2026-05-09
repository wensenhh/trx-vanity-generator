#include "../host/opencl_manager.h"
#include "../utils/constants.h"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <stdexcept>
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
    throw std::runtime_error("Unable to locate vanity.cl for GPU match_count cap test");
}

} // namespace

int main() {
    try {
        constexpr cl_uint batch_size = 64;
        constexpr size_t overdispatched_work_items = batch_size * 8;

        OpenCLManager cl;
        cl.initialize();
        const auto kernel_path = find_kernel_path();
        cl.load_kernel("test_match_count_cap", kernel_path.string());
        cl.build_program("-cl-std=CL1.2 -Werror -DUSE_OPENCL=1");
        auto kernel = cl.get_kernel("test_match_count_cap");

        auto results_buffer = cl.create_buffer(CL_MEM_WRITE_ONLY, batch_size * sizeof(GPUMatchResult));
        auto match_count_buffer = cl.create_buffer(CL_MEM_READ_WRITE, sizeof(cl_uint));

        cl_uint zero = 0;
        cl.write_buffer(match_count_buffer, sizeof(cl_uint), &zero, true);

        cl.set_kernel_arg_buffer(kernel, 0, results_buffer);
        cl.set_kernel_arg_buffer(kernel, 1, match_count_buffer);
        cl.set_kernel_arg(kernel, 2, sizeof(cl_uint), &batch_size);

        size_t global = overdispatched_work_items;
        size_t local = 1;
        cl.enqueue_nd_range(kernel, 1, &global, &local);
        cl.finish();

        cl_uint match_count = 0;
        std::vector<GPUMatchResult> results(batch_size);
        cl.read_buffer(match_count_buffer, sizeof(cl_uint), &match_count, true);
        cl.read_buffer(results_buffer, batch_size * sizeof(GPUMatchResult), results.data(), true);

        cl.release_buffer(results_buffer);
        cl.release_buffer(match_count_buffer);
        clReleaseKernel(kernel);

        if (match_count != batch_size) {
            std::cerr << "match_count cap failed: gpu=" << match_count
                      << " expected=" << batch_size << "\n";
            return 1;
        }

        std::vector<uint32_t> slots;
        slots.reserve(batch_size);
        for (const auto& result : results) {
            slots.push_back(result.reserved[0]);
        }
        std::sort(slots.begin(), slots.end());
        if (std::adjacent_find(slots.begin(), slots.end()) != slots.end()) {
            std::cerr << "match_count cap failed: duplicate result slot payloads\n";
            return 1;
        }

        std::cout << "GPU match_count cap regression: launched=" << overdispatched_work_items
                  << " capacity=" << batch_size
                  << " gpu_matches=" << match_count
                  << " -> OK\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "GPU match_count cap test error: " << e.what() << "\n";
        return 1;
    }
}

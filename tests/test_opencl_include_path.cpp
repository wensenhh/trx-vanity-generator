#include "host/opencl_manager.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;

int main() {
    try {
        auto temp_root = fs::temp_directory_path() / "trx opencl kernel dir [space] #test";
        fs::remove_all(temp_root);
        fs::create_directories(temp_root);

        const auto include_path = temp_root / "included_constants.cl";
        const auto kernel_path = temp_root / "include_path_smoke.cl";

        {
            std::ofstream include_file(include_path);
            include_file << "#define TRX_INCLUDE_PATH_SENTINEL 42\n";
        }
        {
            std::ofstream kernel_file(kernel_path);
            kernel_file
                << "#include \"included_constants.cl\"\n"
                << "__kernel void include_path_smoke(__global uint* out) {\n"
                << "    out[0] = TRX_INCLUDE_PATH_SENTINEL;\n"
                << "}\n";
        }

        trx::OpenCLManager cl;
        cl.initialize();
        cl.load_kernel("include_path_smoke", kernel_path.string());
        cl.build_program();
        cl_kernel kernel = cl.get_kernel("include_path_smoke");
        clReleaseKernel(kernel);

        fs::remove_all(temp_root);
        std::cout << "OpenCL include path with spaces/special chars OK: " << temp_root << "\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "OpenCL include path smoke failed: " << e.what() << "\n";
        return 1;
    }
}

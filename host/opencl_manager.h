#ifndef TRX_OPENCL_MANAGER_H
#define TRX_OPENCL_MANAGER_H

#include "utils/constants.h"
#ifdef __APPLE__
#include <OpenCL/opencl.h>
#else
#include <CL/cl.h>
#endif
#include <string>
#include <vector>
#include <memory>
#include <stdexcept>

namespace trx {

// OpenCL error wrapper
class OpenCLException : public std::runtime_error {
public:
    explicit OpenCLException(const std::string& msg);
    OpenCLException(cl_int err, const std::string& msg);
};

// Device info
struct OpenCLDevice {
    cl_device_id id;
    std::string name;
    std::string vendor;
    std::string version;
    cl_device_type type;
    size_t max_work_group_size;
    cl_ulong global_mem_size;
    cl_ulong local_mem_size;
    cl_uint compute_units;
    cl_uint max_clock_frequency;
};

// Platform info
struct OpenCLPlatform {
    cl_platform_id id;
    std::string name;
    std::string vendor;
    std::string version;
    std::vector<OpenCLDevice> devices;
};

// ============================================================================
// OpenCL Manager - Host-side OpenCL lifecycle management
// ============================================================================

class OpenCLManager {
public:
    OpenCLManager();
    ~OpenCLManager();

    // Initialization
    void initialize();
    void select_device(int platform_idx, int device_idx);
    void select_best_device();

    // Program management
    void load_kernel(const std::string& kernel_name, const std::string& source_path);
    void load_kernel_from_source(const std::string& kernel_name, const std::string& source);
    void build_program(const std::string& options = "-cl-std=CL1.2 -Werror");

    // Kernel execution
    cl_kernel get_kernel(const std::string& name);
    void set_kernel_arg(cl_kernel kernel, cl_uint idx, size_t size, const void* value);
    void set_kernel_arg_buffer(cl_kernel kernel, cl_uint idx, cl_mem buffer);

    // Buffer management
    cl_mem create_buffer(cl_mem_flags flags, size_t size, void* host_ptr = nullptr);
    void release_buffer(cl_mem buffer);
    void write_buffer(cl_mem buffer, size_t size, const void* ptr, bool blocking = true);
    void read_buffer(cl_mem buffer, size_t size, void* ptr, bool blocking = true);

    // Execution
    void enqueue_nd_range(cl_kernel kernel, cl_uint work_dim,
                          const size_t* global_work_size,
                          const size_t* local_work_size);
    void finish();

    // Info
    std::vector<OpenCLPlatform> get_platforms() const { return platforms_; }
    OpenCLDevice get_selected_device() const { return selected_device_; }

    // Utility
    static std::string get_error_string(cl_int err);

private:
    cl_platform_id platform_;
    cl_device_id device_;
    cl_context context_;
    cl_command_queue queue_;
    cl_program program_;

    std::vector<OpenCLPlatform> platforms_;
    OpenCLDevice selected_device_;

    bool initialized_;

    void query_platforms();
    void query_devices(OpenCLPlatform& platform);
};

// RAII wrapper for cl_mem
class Buffer {
public:
    Buffer(cl_mem mem);
    ~Buffer();

    // Disable copy
    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;

    // Enable move
    Buffer(Buffer&& other) noexcept;
    Buffer& operator=(Buffer&& other) noexcept;

    cl_mem get() const { return mem_; }

private:
    cl_mem mem_;
};

} // namespace trx

#endif // TRX_OPENCL_MANAGER_H
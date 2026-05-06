#include "opencl_manager.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>

namespace trx {

// ============================================================================
// OpenCL Exception
// ============================================================================

OpenCLException::OpenCLException(const std::string& msg)
    : std::runtime_error(msg) {}

OpenCLException::OpenCLException(cl_int err, const std::string& msg)
    : std::runtime_error(msg + " (OpenCL error: " + OpenCLManager::get_error_string(err) + ")") {}

std::string OpenCLManager::get_error_string(cl_int err) {
    switch(err) {
        case CL_SUCCESS: return "CL_SUCCESS";
        case CL_DEVICE_NOT_FOUND: return "CL_DEVICE_NOT_FOUND";
        case CL_DEVICE_NOT_AVAILABLE: return "CL_DEVICE_NOT_AVAILABLE";
        case CL_COMPILER_NOT_AVAILABLE: return "CL_COMPILER_NOT_AVAILABLE";
        case CL_MEM_OBJECT_ALLOCATION_FAILURE: return "CL_MEM_OBJECT_ALLOCATION_FAILURE";
        case CL_OUT_OF_RESOURCES: return "CL_OUT_OF_RESOURCES";
        case CL_OUT_OF_HOST_MEMORY: return "CL_OUT_OF_HOST_MEMORY";
        case CL_PROFILING_INFO_NOT_AVAILABLE: return "CL_PROFILING_INFO_NOT_AVAILABLE";
        case CL_MEM_COPY_OVERLAP: return "CL_MEM_COPY_OVERLAP";
        case CL_IMAGE_FORMAT_MISMATCH: return "CL_IMAGE_FORMAT_MISMATCH";
        case CL_IMAGE_FORMAT_NOT_SUPPORTED: return "CL_IMAGE_FORMAT_NOT_SUPPORTED";
        case CL_BUILD_PROGRAM_FAILURE: return "CL_BUILD_PROGRAM_FAILURE";
        case CL_MAP_FAILURE: return "CL_MAP_FAILURE";
        case CL_INVALID_VALUE: return "CL_INVALID_VALUE";
        case CL_INVALID_DEVICE_TYPE: return "CL_INVALID_DEVICE_TYPE";
        case CL_INVALID_PLATFORM: return "CL_INVALID_PLATFORM";
        case CL_INVALID_DEVICE: return "CL_INVALID_DEVICE";
        case CL_INVALID_CONTEXT: return "CL_INVALID_CONTEXT";
        case CL_INVALID_QUEUE_PROPERTIES: return "CL_INVALID_QUEUE_PROPERTIES";
        case CL_INVALID_COMMAND_QUEUE: return "CL_INVALID_COMMAND_QUEUE";
        case CL_INVALID_HOST_PTR: return "CL_INVALID_HOST_PTR";
        case CL_INVALID_MEM_OBJECT: return "CL_INVALID_MEM_OBJECT";
        case CL_INVALID_IMAGE_FORMAT_DESCRIPTOR: return "CL_INVALID_IMAGE_FORMAT_DESCRIPTOR";
        case CL_INVALID_IMAGE_SIZE: return "CL_INVALID_IMAGE_SIZE";
        case CL_INVALID_SAMPLER: return "CL_INVALID_SAMPLER";
        case CL_INVALID_BINARY: return "CL_INVALID_BINARY";
        case CL_INVALID_BUILD_OPTIONS: return "CL_INVALID_BUILD_OPTIONS";
        case CL_INVALID_PROGRAM: return "CL_INVALID_PROGRAM";
        case CL_INVALID_PROGRAM_EXECUTABLE: return "CL_INVALID_PROGRAM_EXECUTABLE";
        case CL_INVALID_KERNEL_NAME: return "CL_INVALID_KERNEL_NAME";
        case CL_INVALID_KERNEL_DEFINITION: return "CL_INVALID_KERNEL_DEFINITION";
        case CL_INVALID_KERNEL: return "CL_INVALID_KERNEL";
        case CL_INVALID_ARG_INDEX: return "CL_INVALID_ARG_INDEX";
        case CL_INVALID_ARG_VALUE: return "CL_INVALID_ARG_VALUE";
        case CL_INVALID_ARG_SIZE: return "CL_INVALID_ARG_SIZE";
        case CL_INVALID_KERNEL_ARGS: return "CL_INVALID_KERNEL_ARGS";
        case CL_INVALID_WORK_DIMENSION: return "CL_INVALID_WORK_DIMENSION";
        case CL_INVALID_WORK_GROUP_SIZE: return "CL_INVALID_WORK_GROUP_SIZE";
        case CL_INVALID_WORK_ITEM_SIZE: return "CL_INVALID_WORK_ITEM_SIZE";
        case CL_INVALID_GLOBAL_OFFSET: return "CL_INVALID_GLOBAL_OFFSET";
        case CL_INVALID_EVENT_WAIT_LIST: return "CL_INVALID_EVENT_WAIT_LIST";
        case CL_INVALID_EVENT: return "CL_INVALID_EVENT";
        case CL_INVALID_OPERATION: return "CL_INVALID_OPERATION";
        case CL_INVALID_GL_OBJECT: return "CL_INVALID_GL_OBJECT";
        case CL_INVALID_BUFFER_SIZE: return "CL_INVALID_BUFFER_SIZE";
        case CL_INVALID_MIP_LEVEL: return "CL_INVALID_MIP_LEVEL";
        case CL_INVALID_GLOBAL_WORK_SIZE: return "CL_INVALID_GLOBAL_WORK_SIZE";
        default: return "Unknown OpenCL error: " + std::to_string(err);
    }
}

// ============================================================================
// OpenCL Manager Implementation
// ============================================================================

OpenCLManager::OpenCLManager()
    : platform_(nullptr), device_(nullptr), context_(nullptr),
      queue_(nullptr), program_(nullptr), initialized_(false) {}

OpenCLManager::~OpenCLManager() {
    if (program_) clReleaseProgram(program_);
    if (queue_) clReleaseCommandQueue(queue_);
    if (context_) clReleaseContext(context_);
}

void OpenCLManager::initialize() {
    query_platforms();

    if (platforms_.empty()) {
        throw OpenCLException("No OpenCL platforms found");
    }

    // Auto-select first platform with GPU
    bool found = false;
    for (auto& platform : platforms_) {
        for (auto& device : platform.devices) {
            if (device.type == CL_DEVICE_TYPE_GPU) {
                select_device(
                    static_cast<int>(&platform - &platforms_[0]),
                    static_cast<int>(&device - &platform.devices[0]));
                found = true;
                break;
            }
        }
        if (found) break;
    }

    if (!found) {
        // Fallback to first device of first platform
        select_device(0, 0);
    }

    initialized_ = true;
}

void OpenCLManager::query_platforms() {
    cl_uint num_platforms;
    cl_int err = clGetPlatformIDs(0, nullptr, &num_platforms);
    if (err != CL_SUCCESS || num_platforms == 0) {
        return;
    }

    std::vector<cl_platform_id> platform_ids(num_platforms);
    clGetPlatformIDs(num_platforms, platform_ids.data(), nullptr);

    for (auto id : platform_ids) {
        OpenCLPlatform platform;
        platform.id = id;

        char buf[1024];
        size_t len;

        clGetPlatformInfo(id, CL_PLATFORM_NAME, sizeof(buf), buf, &len);
        platform.name = std::string(buf, len - 1);

        clGetPlatformInfo(id, CL_PLATFORM_VENDOR, sizeof(buf), buf, &len);
        platform.vendor = std::string(buf, len - 1);

        clGetPlatformInfo(id, CL_PLATFORM_VERSION, sizeof(buf), buf, &len);
        platform.version = std::string(buf, len - 1);

        query_devices(platform);
        platforms_.push_back(std::move(platform));
    }
}

void OpenCLManager::query_devices(OpenCLPlatform& platform) {
    cl_uint num_devices;
    cl_int err = clGetDeviceIDs(platform.id, CL_DEVICE_TYPE_ALL, 0, nullptr, &num_devices);
    if (err != CL_SUCCESS || num_devices == 0) {
        return;
    }

    std::vector<cl_device_id> device_ids(num_devices);
    clGetDeviceIDs(platform.id, CL_DEVICE_TYPE_ALL, num_devices, device_ids.data(), nullptr);

    for (auto id : device_ids) {
        OpenCLDevice device;
        device.id = id;

        char buf[1024];
        size_t len;

        clGetDeviceInfo(id, CL_DEVICE_NAME, sizeof(buf), buf, &len);
        device.name = std::string(buf, len - 1);

        clGetDeviceInfo(id, CL_DEVICE_VENDOR, sizeof(buf), buf, &len);
        device.vendor = std::string(buf, len - 1);

        clGetDeviceInfo(id, CL_DEVICE_VERSION, sizeof(buf), buf, &len);
        device.version = std::string(buf, len - 1);

        clGetDeviceInfo(id, CL_DEVICE_TYPE, sizeof(device.type), &device.type, nullptr);
        clGetDeviceInfo(id, CL_DEVICE_MAX_WORK_GROUP_SIZE, sizeof(size_t), &device.max_work_group_size, nullptr);
        clGetDeviceInfo(id, CL_DEVICE_GLOBAL_MEM_SIZE, sizeof(cl_ulong), &device.global_mem_size, nullptr);
        clGetDeviceInfo(id, CL_DEVICE_LOCAL_MEM_SIZE, sizeof(cl_ulong), &device.local_mem_size, nullptr);
        clGetDeviceInfo(id, CL_DEVICE_MAX_COMPUTE_UNITS, sizeof(cl_uint), &device.compute_units, nullptr);
        clGetDeviceInfo(id, CL_DEVICE_MAX_CLOCK_FREQUENCY, sizeof(cl_uint), &device.max_clock_frequency, nullptr);

        platform.devices.push_back(std::move(device));
    }
}

void OpenCLManager::select_device(int platform_idx, int device_idx) {
    if (platform_idx < 0 || platform_idx >= static_cast<int>(platforms_.size())) {
        throw OpenCLException("Invalid platform index");
    }
    if (device_idx < 0 || device_idx >= static_cast<int>(platforms_[platform_idx].devices.size())) {
        throw OpenCLException("Invalid device index");
    }

    platform_ = platforms_[platform_idx].id;
    device_ = platforms_[platform_idx].devices[device_idx].id;
    selected_device_ = platforms_[platform_idx].devices[device_idx];

    cl_int err;
    context_ = clCreateContext(nullptr, 1, &device_, nullptr, nullptr, &err);
    if (err != CL_SUCCESS) {
        throw OpenCLException(err, "Failed to create OpenCL context");
    }

#ifdef CL_VERSION_2_0
    const cl_queue_properties props[] = {
        CL_QUEUE_PROPERTIES, CL_QUEUE_PROFILING_ENABLE,
        0
    };
    queue_ = clCreateCommandQueueWithProperties(context_, device_, props, &err);
#else
    queue_ = clCreateCommandQueue(context_, device_, CL_QUEUE_PROFILING_ENABLE, &err);
#endif
    if (err != CL_SUCCESS) {
        throw OpenCLException(err, "Failed to create command queue");
    }
}

void OpenCLManager::select_best_device() {
    // Select device with most compute units
    const OpenCLDevice* best = nullptr;
    int best_platform = 0;
    int best_device = 0;

    for (int pi = 0; pi < static_cast<int>(platforms_.size()); ++pi) {
        for (int di = 0; di < static_cast<int>(platforms_[pi].devices.size()); ++di) {
            const auto& dev = platforms_[pi].devices[di];
            if (!best || dev.compute_units > best->compute_units) {
                best = &dev;
                best_platform = pi;
                best_device = di;
            }
        }
    }

    if (best) {
        select_device(best_platform, best_device);
    }
}

void OpenCLManager::load_kernel(const std::string& kernel_name, const std::string& source_path) {
    std::ifstream file(source_path);
    if (!file.is_open()) {
        throw OpenCLException("Failed to open kernel file: " + source_path);
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string source = buffer.str();

    const char* source_ptr = source.c_str();
    size_t source_len = source.length();

    cl_int err;
    program_ = clCreateProgramWithSource(context_, 1, &source_ptr, &source_len, &err);
    if (err != CL_SUCCESS) {
        throw OpenCLException(err, "Failed to create program");
    }
}

void OpenCLManager::load_kernel_from_source(const std::string& kernel_name, const std::string& source) {
    const char* source_ptr = source.c_str();
    size_t source_len = source.length();

    cl_int err;
    program_ = clCreateProgramWithSource(context_, 1, &source_ptr, &source_len, &err);
    if (err != CL_SUCCESS) {
        throw OpenCLException(err, "Failed to create program from source");
    }
}

void OpenCLManager::build_program(const std::string& options) {
    cl_int err = clBuildProgram(program_, 1, &device_, options.c_str(), nullptr, nullptr);
    if (err != CL_SUCCESS) {
        // Get build log
        size_t log_size;
        clGetProgramBuildInfo(program_, device_, CL_PROGRAM_BUILD_LOG, 0, nullptr, &log_size);
        std::vector<char> log(log_size);
        clGetProgramBuildInfo(program_, device_, CL_PROGRAM_BUILD_LOG, log_size, log.data(), nullptr);

        throw OpenCLException(err, std::string("Program build failed:\n") + log.data());
    }
}

cl_kernel OpenCLManager::get_kernel(const std::string& name) {
    cl_int err;
    cl_kernel kernel = clCreateKernel(program_, name.c_str(), &err);
    if (err != CL_SUCCESS) {
        throw OpenCLException(err, "Failed to create kernel: " + name);
    }
    return kernel;
}

void OpenCLManager::set_kernel_arg(cl_kernel kernel, cl_uint idx, size_t size, const void* value) {
    cl_int err = clSetKernelArg(kernel, idx, size, value);
    if (err != CL_SUCCESS) {
        throw OpenCLException(err, "Failed to set kernel arg " + std::to_string(idx));
    }
}

void OpenCLManager::set_kernel_arg_buffer(cl_kernel kernel, cl_uint idx, cl_mem buffer) {
    set_kernel_arg(kernel, idx, sizeof(cl_mem), &buffer);
}

cl_mem OpenCLManager::create_buffer(cl_mem_flags flags, size_t size, void* host_ptr) {
    cl_int err;
    cl_mem buffer = clCreateBuffer(context_, flags, size, host_ptr, &err);
    if (err != CL_SUCCESS) {
        throw OpenCLException(err, "Failed to create buffer of size " + std::to_string(size));
    }
    return buffer;
}

void OpenCLManager::release_buffer(cl_mem buffer) {
    if (buffer) clReleaseMemObject(buffer);
}

void OpenCLManager::write_buffer(cl_mem buffer, size_t size, const void* ptr, bool blocking) {
    cl_int err = clEnqueueWriteBuffer(queue_, buffer, blocking ? CL_TRUE : CL_FALSE,
                                      0, size, ptr, 0, nullptr, nullptr);
    if (err != CL_SUCCESS) {
        throw OpenCLException(err, "Failed to write buffer");
    }
}

void OpenCLManager::read_buffer(cl_mem buffer, size_t size, void* ptr, bool blocking) {
    cl_int err = clEnqueueReadBuffer(queue_, buffer, blocking ? CL_TRUE : CL_FALSE,
                                     0, size, ptr, 0, nullptr, nullptr);
    if (err != CL_SUCCESS) {
        throw OpenCLException(err, "Failed to read buffer");
    }
}

void OpenCLManager::enqueue_nd_range(cl_kernel kernel, cl_uint work_dim,
                                     const size_t* global_work_size,
                                     const size_t* local_work_size) {
    cl_int err = clEnqueueNDRangeKernel(queue_, kernel, work_dim, nullptr,
                                          global_work_size, local_work_size,
                                          0, nullptr, nullptr);
    if (err != CL_SUCCESS) {
        throw OpenCLException(err, "Failed to enqueue NDRange kernel");
    }
}

double OpenCLManager::enqueue_nd_range_timed_ms(cl_kernel kernel, cl_uint work_dim,
                                                const size_t* global_work_size,
                                                const size_t* local_work_size) {
    cl_event event = nullptr;
    cl_int err = clEnqueueNDRangeKernel(queue_, kernel, work_dim, nullptr,
                                          global_work_size, local_work_size,
                                          0, nullptr, &event);
    if (err != CL_SUCCESS) {
        throw OpenCLException(err, "Failed to enqueue timed NDRange kernel");
    }

    err = clWaitForEvents(1, &event);
    if (err != CL_SUCCESS) {
        clReleaseEvent(event);
        throw OpenCLException(err, "Failed waiting for timed NDRange kernel");
    }

    cl_ulong start_ns = 0;
    cl_ulong end_ns = 0;
    err = clGetEventProfilingInfo(event, CL_PROFILING_COMMAND_START, sizeof(start_ns), &start_ns, nullptr);
    if (err != CL_SUCCESS) {
        clReleaseEvent(event);
        throw OpenCLException(err, "Failed reading kernel profiling start time");
    }
    err = clGetEventProfilingInfo(event, CL_PROFILING_COMMAND_END, sizeof(end_ns), &end_ns, nullptr);
    if (err != CL_SUCCESS) {
        clReleaseEvent(event);
        throw OpenCLException(err, "Failed reading kernel profiling end time");
    }

    clReleaseEvent(event);
    return static_cast<double>(end_ns - start_ns) / 1000000.0;
}

void OpenCLManager::finish() {
    clFinish(queue_);
}

// ============================================================================
// Buffer RAII
// ============================================================================

Buffer::Buffer(cl_mem mem) : mem_(mem) {}

Buffer::~Buffer() {
    if (mem_) clReleaseMemObject(mem_);
}

Buffer::Buffer(Buffer&& other) noexcept : mem_(other.mem_) {
    other.mem_ = nullptr;
}

Buffer& Buffer::operator=(Buffer&& other) noexcept {
    if (this != &other) {
        if (mem_) clReleaseMemObject(mem_);
        mem_ = other.mem_;
        other.mem_ = nullptr;
    }
    return *this;
}

} // namespace trx
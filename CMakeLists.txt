cmake_minimum_required(VERSION 3.16)
project(trx_vanity VERSION 1.0.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Options
option(USE_OPENCL "Enable OpenCL GPU acceleration" ON)
option(BUILD_TESTS "Build unit tests" OFF)

# Find packages
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)

if(USE_OPENCL)
    find_package(OpenCL QUIET)
    if(NOT OpenCL_FOUND)
        # Manual search for macOS and Windows
        if(APPLE)
            set(OpenCL_INCLUDE_DIRS "/System/Library/Frameworks/OpenCL.framework/Headers")
            set(OpenCL_LIBRARIES "-framework OpenCL")
            set(OpenCL_FOUND TRUE)
        elseif(WIN32)
            # Search common NVIDIA/AMD OpenCL paths
            find_path(OpenCL_INCLUDE_DIRS CL/cl.h
                PATHS
                    "C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/*/include"
                    "C:/Program Files (x86)/AMD APP SDK/*/include"
                    "C:/Intel/OpenCL/sdk/include"
            )
            find_library(OpenCL_LIBRARIES OpenCL
                PATHS
                    "C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/*/lib/x64"
                    "C:/Program Files (x86)/AMD APP SDK/*/lib/x86_64"
                    "C:/Intel/OpenCL/sdk/lib/x64"
            )
            if(OpenCL_INCLUDE_DIRS AND OpenCL_LIBRARIES)
                set(OpenCL_FOUND TRUE)
            endif()
        endif()
    endif()
endif()

# Source files
set(HOST_SOURCES
    host/cpu_generator.cpp
)

set(UTIL_SOURCES
    utils/crypto.cpp
    utils/pattern.cpp
    utils/rng.cpp
    utils/estimate.cpp
)

set(ALL_SOURCES
    main.cpp
    ${HOST_SOURCES}
    ${UTIL_SOURCES}
)

# OpenCL sources
if(OpenCL_FOUND AND USE_OPENCL)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/host/opencl_manager.cpp")
        list(APPEND ALL_SOURCES
            host/opencl_manager.cpp
            host/gpu_generator.cpp
        )
    endif()
    add_definitions(-DUSE_OPENCL=1)
    include_directories(${OpenCL_INCLUDE_DIRS})
endif()

# Executable
add_executable(trx_vanity ${ALL_SOURCES})

target_link_libraries(trx_vanity
    OpenSSL::SSL
    OpenSSL::Crypto
    Threads::Threads
)

if(OpenCL_FOUND AND USE_OPENCL)
    target_link_libraries(trx_vanity ${OpenCL_LIBRARIES})
endif()

# Include directories
target_include_directories(trx_vanity PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Compiler flags
if(MSVC)
    target_compile_options(trx_vanity PRIVATE /W4 /O2)
else()
    target_compile_options(trx_vanity PRIVATE -Wall -Wextra -O3 -march=native)
endif()

# Copy OpenCL kernels to build directory
if(OpenCL_FOUND AND USE_OPENCL)
    file(GLOB KERNEL_FILES "kernel/*.cl")
    foreach(kernel ${KERNEL_FILES})
        get_filename_component(kernel_name ${kernel} NAME)
        configure_file(${kernel} ${CMAKE_CURRENT_BINARY_DIR}/${kernel_name} COPYONLY)
    endforeach()
endif()

# ECC/GPU test executables
if(OpenCL_FOUND AND USE_OPENCL)
    add_executable(test_ecc tests/test_ecc.cpp utils/crypto.cpp utils/rng.cpp host/opencl_manager.cpp)
    target_link_libraries(test_ecc OpenSSL::SSL OpenSSL::Crypto Threads::Threads ${OpenCL_LIBRARIES})
    target_include_directories(test_ecc PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${OpenCL_INCLUDE_DIRS})
    target_compile_options(test_ecc PRIVATE -Wall -Wextra -O3 -march=native)

    add_executable(test_gpu_filter tests/test_gpu_filter.cpp utils/crypto.cpp host/opencl_manager.cpp)
    target_link_libraries(test_gpu_filter OpenSSL::SSL OpenSSL::Crypto Threads::Threads ${OpenCL_LIBRARIES})
    target_include_directories(test_gpu_filter PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${OpenCL_INCLUDE_DIRS})
    target_compile_options(test_gpu_filter PRIVATE -Wall -Wextra -O3 -march=native)

    add_executable(test_gpu_full_pipeline tests/test_gpu_full_pipeline.cpp utils/crypto.cpp utils/rng.cpp host/opencl_manager.cpp)
    target_link_libraries(test_gpu_full_pipeline OpenSSL::SSL OpenSSL::Crypto Threads::Threads ${OpenCL_LIBRARIES})
    target_include_directories(test_gpu_full_pipeline PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${OpenCL_INCLUDE_DIRS})
    target_compile_options(test_gpu_full_pipeline PRIVATE -Wall -Wextra -O3 -march=native)
endif()

# Tests
if(BUILD_TESTS)
    enable_testing()

    add_executable(test_estimate tests/test_estimate.cpp utils/estimate.cpp)
    target_include_directories(test_estimate PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_compile_options(test_estimate PRIVATE -Wall -Wextra -O3 -march=native)
    add_test(NAME estimate_difficulty COMMAND test_estimate)

    if(TARGET test_ecc)
        add_test(NAME ecc_correctness COMMAND test_ecc)
    endif()
    if(TARGET test_gpu_filter)
        add_test(NAME gpu_base58_filter COMMAND test_gpu_filter)
    endif()
    if(TARGET test_gpu_full_pipeline)
        add_test(NAME gpu_full_pipeline_deterministic COMMAND test_gpu_full_pipeline)
        set_tests_properties(gpu_full_pipeline_deterministic PROPERTIES TIMEOUT 20)
    endif()

    add_test(NAME cli_cpu_smoke
        COMMAND trx_vanity contains T --max-attempts 64 -t 1)
    set_tests_properties(cli_cpu_smoke PROPERTIES TIMEOUT 10)

    add_test(NAME cli_cpu_suffix_smoke
        COMMAND trx_vanity suffix 8888 --max-attempts 64 -t 1)
    set_tests_properties(cli_cpu_suffix_smoke PROPERTIES TIMEOUT 10)

    add_test(NAME cli_cpu_prefix_smoke
        COMMAND trx_vanity prefix ABC --max-attempts 64 -t 1)
    set_tests_properties(cli_cpu_prefix_smoke PROPERTIES TIMEOUT 10)

    add_test(NAME cli_cpu_consecutive_smoke
        COMMAND trx_vanity consecutive 8 4 --max-attempts 64 -t 1)
    set_tests_properties(cli_cpu_consecutive_smoke PROPERTIES TIMEOUT 10)

    add_test(NAME cli_cpu_sequential_smoke
        COMMAND trx_vanity sequential 1 4 --max-attempts 64 -t 1)
    set_tests_properties(cli_cpu_sequential_smoke PROPERTIES TIMEOUT 10)

    configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_private_key_output.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/cli_private_key_output.cmake
        @ONLY)
    add_test(NAME cli_private_key_output_security
        COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cli_private_key_output.cmake)
    set_tests_properties(cli_private_key_output_security PROPERTIES TIMEOUT 20)

    configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_argument_validation.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/cli_argument_validation.cmake
        @ONLY)
    add_test(NAME cli_argument_validation
        COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cli_argument_validation.cmake)
    set_tests_properties(cli_argument_validation PROPERTIES TIMEOUT 10)

    configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/install_content_smoke.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/install_content_smoke.cmake
        @ONLY)
    add_test(NAME install_package_content_smoke
        COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/install_content_smoke.cmake)
    set_tests_properties(install_package_content_smoke PROPERTIES TIMEOUT 30)

    if(TARGET test_ecc)
        add_test(NAME cli_gpu_smoke
            COMMAND trx_vanity contains T --gpu --batch-size 64 --batches 1)
        set_tests_properties(cli_gpu_smoke PROPERTIES TIMEOUT 20)

        add_test(NAME cli_gpu_autotune_smoke
            COMMAND trx_vanity prefix ZZZ --gpu --auto-tune --auto-tune-sizes 64,128 --auto-tune-batches 1 --batches 1 --benchmark-json)
        set_tests_properties(cli_gpu_autotune_smoke PROPERTIES TIMEOUT 30)

        add_test(NAME cli_gpu_profile_smoke
            COMMAND trx_vanity contains T --gpu --batch-size 64 --batches 1 --profile --benchmark-json)
        set_tests_properties(cli_gpu_profile_smoke PROPERTIES TIMEOUT 30)

        configure_file(
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/install_tree_smoke.cmake.in
            ${CMAKE_CURRENT_BINARY_DIR}/install_tree_smoke.cmake
            @ONLY)
        add_test(NAME install_tree_gpu_runtime_smoke
            COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/install_tree_smoke.cmake)
        set_tests_properties(install_tree_gpu_runtime_smoke PROPERTIES TIMEOUT 40)
    endif()
endif()

# Install/package layout
#
# Keep the install tree friendly for non-developer release archives:
#   bin/trx_vanity
#   README.md
#   share/trx_vanity/docs/*.md
#   share/trx_vanity/kernel/*.cl
install(TARGETS trx_vanity DESTINATION bin)
install(FILES README.md DESTINATION .)
install(FILES
    docs/USER_GUIDE_zh.md
    docs/SECURITY_zh.md
    docs/WEBSITE_COPY_zh.md
    docs/PACKAGING_zh.md
    DESTINATION share/trx_vanity/docs)
if(OpenCL_FOUND AND USE_OPENCL)
    install(DIRECTORY kernel/
        DESTINATION share/trx_vanity/kernel
        FILES_MATCHING PATTERN "*.cl")
endif()

set(CPACK_PACKAGE_NAME "trx_vanity")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Local/offline TRON vanity address generator with OpenCL GPU acceleration")
set(CPACK_PACKAGE_VENDOR "trx-vanity-generator")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "trx_vanity")
set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")
set(CPACK_GENERATOR "TGZ;ZIP")
set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
include(CPack)

cmake_minimum_required(VERSION 3.16)
project(trx_vanity VERSION 1.0.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Options
option(USE_OPENCL "Enable OpenCL GPU acceleration" ON)
option(BUILD_TESTS "Build unit tests" OFF)
option(BUILD_GUI "Build desktop GUI (FLTK)" ON)

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
    utils/export_encryption.cpp
    utils/history.cpp
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

if(APPLE)
    add_executable(trx_vanity_app MACOSX_BUNDLE
        packaging/macos/trx_vanity_app.cpp)
    set_target_properties(trx_vanity_app PROPERTIES
        OUTPUT_NAME "TRX Vanity"
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/packaging/macos/Info.plist.in")
endif()

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

    add_executable(test_gpu_match_count_cap tests/test_gpu_match_count_cap.cpp host/opencl_manager.cpp)
    target_link_libraries(test_gpu_match_count_cap Threads::Threads ${OpenCL_LIBRARIES})
    target_include_directories(test_gpu_match_count_cap PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${OpenCL_INCLUDE_DIRS})
    target_compile_options(test_gpu_match_count_cap PRIVATE -Wall -Wextra -O3 -march=native)

    add_executable(test_opencl_include_path tests/test_opencl_include_path.cpp host/opencl_manager.cpp)
    target_link_libraries(test_opencl_include_path Threads::Threads ${OpenCL_LIBRARIES})
    target_include_directories(test_opencl_include_path PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${OpenCL_INCLUDE_DIRS})
    target_compile_options(test_opencl_include_path PRIVATE -Wall -Wextra -O3 -march=native)
endif()

# Tests
if(BUILD_TESTS)
    enable_testing()

    add_executable(test_estimate tests/test_estimate.cpp utils/estimate.cpp)
    target_include_directories(test_estimate PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_compile_options(test_estimate PRIVATE -Wall -Wextra -O3 -march=native)
    add_test(NAME estimate_difficulty COMMAND test_estimate)

    add_executable(test_export_encryption tests/test_export_encryption.cpp utils/export_encryption.cpp)
    target_link_libraries(test_export_encryption OpenSSL::Crypto)
    target_include_directories(test_export_encryption PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_compile_options(test_export_encryption PRIVATE -Wall -Wextra -O3 -march=native)
    add_test(NAME export_encryption COMMAND test_export_encryption)

    add_executable(test_history tests/test_history.cpp utils/history.cpp utils/export_encryption.cpp)
    target_link_libraries(test_history OpenSSL::Crypto)
    target_include_directories(test_history PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_compile_options(test_history PRIVATE -Wall -Wextra -O3 -march=native)
    add_test(NAME history COMMAND test_history)

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
    if(TARGET test_gpu_match_count_cap)
        add_test(NAME gpu_match_count_cap COMMAND test_gpu_match_count_cap)
        set_tests_properties(gpu_match_count_cap PROPERTIES TIMEOUT 20)
    endif()
    if(TARGET test_opencl_include_path)
        add_test(NAME opencl_include_path_with_spaces COMMAND test_opencl_include_path)
        set_tests_properties(opencl_include_path_with_spaces PROPERTIES TIMEOUT 20)
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

    configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/macos_packaging_files_smoke.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/macos_packaging_files_smoke.cmake
        @ONLY)
    add_test(NAME macos_packaging_files_smoke
        COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/macos_packaging_files_smoke.cmake)
    set_tests_properties(macos_packaging_files_smoke PROPERTIES TIMEOUT 10)

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

        configure_file(
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/package_gpu_runtime_smoke.cmake.in
            ${CMAKE_CURRENT_BINARY_DIR}/package_gpu_runtime_smoke.cmake
            @ONLY)
        add_test(NAME package_tgz_gpu_runtime_smoke
            COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/package_gpu_runtime_smoke.cmake)
        set_tests_properties(package_tgz_gpu_runtime_smoke PROPERTIES TIMEOUT 60)
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
    docs/RELEASE_CHECKLIST_zh.md
    docs/RELEASE_FEEDBACK_TEMPLATE_zh.md
    DESTINATION share/trx_vanity/docs)
if(APPLE)
    install(FILES
        packaging/macos/README_zh.md
        packaging/macos/sign_and_notarize_zh.md
        DESTINATION share/trx_vanity/docs/macos)
endif()
if(OpenCL_FOUND AND USE_OPENCL)
    install(DIRECTORY kernel/
        DESTINATION share/trx_vanity/kernel
        FILES_MATCHING PATTERN "*.cl")
endif()

if(APPLE)
    install(TARGETS trx_vanity_app BUNDLE DESTINATION .)
    install(PROGRAMS $<TARGET_FILE:trx_vanity>
        DESTINATION "TRX Vanity.app/Contents/Resources/bin")
    install(FILES README.md
        DESTINATION "TRX Vanity.app/Contents/Resources")
    install(FILES packaging/macos/README_zh.md
        packaging/macos/sign_and_notarize_zh.md
        DESTINATION "TRX Vanity.app/Contents/Resources")
    install(FILES
        docs/USER_GUIDE_zh.md
        docs/SECURITY_zh.md
        docs/WEBSITE_COPY_zh.md
        docs/PACKAGING_zh.md
        docs/RELEASE_CHECKLIST_zh.md
        docs/RELEASE_FEEDBACK_TEMPLATE_zh.md
        DESTINATION "TRX Vanity.app/Contents/Resources/docs")
    if(OpenCL_FOUND AND USE_OPENCL)
        install(DIRECTORY kernel/
            DESTINATION "TRX Vanity.app/Contents/Resources/kernel"
            FILES_MATCHING PATTERN "*.cl")
    endif()
endif()

# ============================================================================
# GUI target (FLTK)
# ============================================================================
if(BUILD_GUI)
    find_package(FLTK QUIET)
    if(NOT FLTK_FOUND)
        # Try pkg-config fallback
        find_package(PkgConfig QUIET)
        if(PkgConfig_FOUND)
            pkg_check_modules(FLTK fltk QUIET)
        endif()
    endif()
    if(NOT FLTK_FOUND)
        # Manual search common paths
        find_path(FLTK_INCLUDE_DIRS FL/Fl.H
            PATHS /usr/include /usr/local/include /opt/homebrew/include /opt/local/include)
        find_library(FLTK_LIBRARIES fltk
            PATHS /usr/lib /usr/local/lib /opt/homebrew/lib /opt/local/lib)
        if(FLTK_INCLUDE_DIRS AND FLTK_LIBRARIES)
            set(FLTK_FOUND TRUE)
        endif()
    endif()

    if(FLTK_FOUND)
        message(STATUS "FLTK found — GUI target enabled")
        set(GUI_SOURCES
            gui/main_gui.cpp
            gui/gui_engine.cpp
            ${HOST_SOURCES}
            ${UTIL_SOURCES}
        )
        if(OpenCL_FOUND AND USE_OPENCL)
            if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/host/opencl_manager.cpp")
                list(APPEND GUI_SOURCES
                    host/opencl_manager.cpp
                    host/gpu_generator.cpp
                )
            endif()
        endif()
        add_executable(trx_vanity_gui ${GUI_SOURCES})
        target_link_libraries(trx_vanity_gui
            OpenSSL::SSL
            OpenSSL::Crypto
            Threads::Threads
            ${FLTK_LIBRARIES}
        )
        if(OpenCL_FOUND AND USE_OPENCL)
            target_link_libraries(trx_vanity_gui ${OpenCL_LIBRARIES})
        endif()
        target_include_directories(trx_vanity_gui PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}
            ${FLTK_INCLUDE_DIRS}
        )
        if(MSVC)
            target_compile_options(trx_vanity_gui PRIVATE /W4 /O2)
        else()
            target_compile_options(trx_vanity_gui PRIVATE -Wall -Wextra -O3 -march=native)
        endif()
        if(APPLE)
            # macOS may need Cocoa framework for FLTK
            find_library(COCOA_LIBRARY Cocoa)
            if(COCOA_LIBRARY)
                target_link_libraries(trx_vanity_gui ${COCOA_LIBRARY})
            endif()
        endif()
        install(TARGETS trx_vanity_gui DESTINATION bin)
    else()
        message(WARNING "FLTK not found — GUI target disabled. Install FLTK or set BUILD_GUI=OFF.")
    endif()
endif()

# ============================================================================
# GUI smoke test (headless — exercises GUIEngine without display)
# ============================================================================
if(BUILD_TESTS AND BUILD_GUI AND FLTK_FOUND)
    add_executable(test_gui_smoke tests/test_gui_smoke.cpp gui/gui_engine.cpp ${HOST_SOURCES} ${UTIL_SOURCES})
    target_link_libraries(test_gui_smoke OpenSSL::SSL OpenSSL::Crypto Threads::Threads)
    target_include_directories(test_gui_smoke PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/gui ${FLTK_INCLUDE_DIRS})
    target_compile_options(test_gui_smoke PRIVATE -Wall -Wextra -O3 -march=native)
    if(OpenCL_FOUND AND USE_OPENCL)
        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/host/opencl_manager.cpp")
            target_sources(test_gui_smoke PRIVATE host/opencl_manager.cpp host/gpu_generator.cpp)
            target_link_libraries(test_gui_smoke ${OpenCL_LIBRARIES})
            target_include_directories(test_gui_smoke PRIVATE ${OpenCL_INCLUDE_DIRS})
        endif()
    endif()
    add_test(NAME gui_engine_smoke COMMAND test_gui_smoke)
    set_tests_properties(gui_engine_smoke PROPERTIES TIMEOUT 30)
endif()

# ============================================================================
# CPack packaging configuration
# ============================================================================
set(CPACK_PACKAGE_NAME "trx_vanity")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Local/offline TRON vanity address generator with OpenCL GPU acceleration")
set(CPACK_PACKAGE_VENDOR "trx-vanity-generator")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "trx_vanity")
set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")
set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/README.md")

set(CPACK_GENERATOR "TGZ;ZIP")
if(APPLE)
    list(APPEND CPACK_GENERATOR "DragNDrop")
    set(CPACK_DMG_VOLUME_NAME "TRX Vanity ${PROJECT_VERSION}")
    set(CPACK_DMG_FORMAT "UDZO")
endif()

if(WIN32)
    list(APPEND CPACK_GENERATOR "NSIS")
    # NSIS package metadata
    set(CPACK_NSIS_PACKAGE_NAME "TRX Vanity Generator")
    set(CPACK_NSIS_DISPLAY_NAME "TRX Vanity Generator ${PROJECT_VERSION}")
    set(CPACK_NSIS_HELP_LINK "https://github.com/wensenhh/trx-vanity-generator")
    set(CPACK_NSIS_URL_INFO_ABOUT "https://github.com/wensenhh/trx-vanity-generator")
    # Add bin directory to user PATH
    set(CPACK_NSIS_MODIFY_PATH ON)
    # Uninstall existing version before reinstalling
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
    # Show "Run TRX Vanity" checkbox on finish page
    set(CPACK_NSIS_MUI_FINISHPAGE_RUN "bin/trx_vanity_gui.bat")
    # Extra NSIS commands: create Start Menu shortcuts and Desktop shortcut
    set(CPACK_NSIS_EXTRA_INSTALL_COMMANDS "
        CreateDirectory '$SMPROGRAMS\\TRX Vanity'
        CreateShortcut '$SMPROGRAMS\\TRX Vanity\\TRX Vanity (GUI Launcher).lnk' '$INSTDIR\\bin\\trx_vanity_gui.bat' '' '$INSTDIR\\bin\\trx_vanity.exe' 0
        CreateShortcut '$SMPROGRAMS\\TRX Vanity\\TRX Vanity CLI.lnk' '$INSTDIR\\bin\\trx_vanity.exe' '' '$INSTDIR\\bin\\trx_vanity.exe' 0
        CreateShortcut '$SMPROGRAMS\\TRX Vanity\\Uninstall.lnk' '$INSTDIR\\uninstall.exe'
        CreateShortcut '$DESKTOP\\TRX Vanity.lnk' '$INSTDIR\\bin\\trx_vanity_gui.bat' '' '$INSTDIR\\bin\\trx_vanity.exe' 0
    ")
    # Extra NSIS commands: remove shortcuts on uninstall
    set(CPACK_NSIS_EXTRA_UNINSTALL_COMMANDS "
        Delete '$SMPROGRAMS\\TRX Vanity\\TRX Vanity (GUI Launcher).lnk'
        Delete '$SMPROGRAMS\\TRX Vanity\\TRX Vanity CLI.lnk'
        Delete '$SMPROGRAMS\\TRX Vanity\\Uninstall.lnk'
        RMDir '$SMPROGRAMS\\TRX Vanity'
        Delete '$DESKTOP\\TRX Vanity.lnk'
    ")
endif()

# Install Windows launcher script so it ends up in bin/ for NSIS/ZIP packages
if(WIN32)
    install(PROGRAMS "${CMAKE_CURRENT_SOURCE_DIR}/packaging/windows/trx_vanity_gui.bat" DESTINATION bin)
endif()

include(CPack)

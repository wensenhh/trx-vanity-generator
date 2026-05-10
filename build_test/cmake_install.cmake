# Install script for directory: /Users/vincen/Vincen/code/trx_addr

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "/usr/local")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "FALSE")
endif()

# Set path to fallback-tool for dependency-resolution.
if(NOT DEFINED CMAKE_OBJDUMP)
  set(CMAKE_OBJDUMP "/usr/bin/objdump")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE EXECUTABLE FILES "/Users/vincen/Vincen/code/trx_addr/build_test/trx_vanity")
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/trx_vanity" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/trx_vanity")
    if(CMAKE_INSTALL_DO_STRIP)
      execute_process(COMMAND "/usr/bin/strip" -u -r "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/trx_vanity")
    endif()
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  include("/Users/vincen/Vincen/code/trx_addr/build_test/CMakeFiles/trx_vanity.dir/install-cxx-module-bmi-noconfig.cmake" OPTIONAL)
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/." TYPE FILE FILES "/Users/vincen/Vincen/code/trx_addr/README.md")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/share/trx_vanity/docs" TYPE FILE FILES
    "/Users/vincen/Vincen/code/trx_addr/docs/USER_GUIDE_zh.md"
    "/Users/vincen/Vincen/code/trx_addr/docs/SECURITY_zh.md"
    "/Users/vincen/Vincen/code/trx_addr/docs/WEBSITE_COPY_zh.md"
    "/Users/vincen/Vincen/code/trx_addr/docs/PACKAGING_zh.md"
    "/Users/vincen/Vincen/code/trx_addr/docs/RELEASE_CHECKLIST_zh.md"
    "/Users/vincen/Vincen/code/trx_addr/docs/RELEASE_FEEDBACK_TEMPLATE_zh.md"
    )
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/share/trx_vanity/docs/macos" TYPE FILE FILES
    "/Users/vincen/Vincen/code/trx_addr/packaging/macos/README_zh.md"
    "/Users/vincen/Vincen/code/trx_addr/packaging/macos/sign_and_notarize_zh.md"
    )
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/." TYPE DIRECTORY FILES "/Users/vincen/Vincen/code/trx_addr/build_test/TRX Vanity.app" USE_SOURCE_PERMISSIONS)
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/TRX Vanity.app/Contents/Resources/bin" TYPE PROGRAM FILES "/Users/vincen/Vincen/code/trx_addr/build_test/trx_vanity")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/TRX Vanity.app/Contents/Resources" TYPE FILE FILES "/Users/vincen/Vincen/code/trx_addr/README.md")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/TRX Vanity.app/Contents/Resources" TYPE FILE FILES
    "/Users/vincen/Vincen/code/trx_addr/packaging/macos/README_zh.md"
    "/Users/vincen/Vincen/code/trx_addr/packaging/macos/sign_and_notarize_zh.md"
    )
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/TRX Vanity.app/Contents/Resources/docs" TYPE FILE FILES
    "/Users/vincen/Vincen/code/trx_addr/docs/USER_GUIDE_zh.md"
    "/Users/vincen/Vincen/code/trx_addr/docs/SECURITY_zh.md"
    "/Users/vincen/Vincen/code/trx_addr/docs/WEBSITE_COPY_zh.md"
    "/Users/vincen/Vincen/code/trx_addr/docs/PACKAGING_zh.md"
    "/Users/vincen/Vincen/code/trx_addr/docs/RELEASE_CHECKLIST_zh.md"
    "/Users/vincen/Vincen/code/trx_addr/docs/RELEASE_FEEDBACK_TEMPLATE_zh.md"
    )
endif()

string(REPLACE ";" "\n" CMAKE_INSTALL_MANIFEST_CONTENT
       "${CMAKE_INSTALL_MANIFEST_FILES}")
if(CMAKE_INSTALL_LOCAL_ONLY)
  file(WRITE "/Users/vincen/Vincen/code/trx_addr/build_test/install_local_manifest.txt"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
endif()
if(CMAKE_INSTALL_COMPONENT)
  if(CMAKE_INSTALL_COMPONENT MATCHES "^[a-zA-Z0-9_.+-]+$")
    set(CMAKE_INSTALL_MANIFEST "install_manifest_${CMAKE_INSTALL_COMPONENT}.txt")
  else()
    string(MD5 CMAKE_INST_COMP_HASH "${CMAKE_INSTALL_COMPONENT}")
    set(CMAKE_INSTALL_MANIFEST "install_manifest_${CMAKE_INST_COMP_HASH}.txt")
    unset(CMAKE_INST_COMP_HASH)
  endif()
else()
  set(CMAKE_INSTALL_MANIFEST "install_manifest.txt")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  file(WRITE "/Users/vincen/Vincen/code/trx_addr/build_test/${CMAKE_INSTALL_MANIFEST}"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
endif()

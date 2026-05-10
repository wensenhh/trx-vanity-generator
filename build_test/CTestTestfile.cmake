# CMake generated Testfile for 
# Source directory: /Users/vincen/Vincen/code/trx_addr
# Build directory: /Users/vincen/Vincen/code/trx_addr/build_test
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(estimate_difficulty "/Users/vincen/Vincen/code/trx_addr/build_test/test_estimate")
set_tests_properties(estimate_difficulty PROPERTIES  _BACKTRACE_TRIPLES "/Users/vincen/Vincen/code/trx_addr/CMakeLists.txt;154;add_test;/Users/vincen/Vincen/code/trx_addr/CMakeLists.txt;0;")
add_test(export_encryption "/Users/vincen/Vincen/code/trx_addr/build_test/test_export_encryption")
set_tests_properties(export_encryption PROPERTIES  _BACKTRACE_TRIPLES "/Users/vincen/Vincen/code/trx_addr/CMakeLists.txt;160;add_test;/Users/vincen/Vincen/code/trx_addr/CMakeLists.txt;0;")
add_test(history "/Users/vincen/Vincen/code/trx_addr/build_test/test_history")
set_tests_properties(history PROPERTIES  _BACKTRACE_TRIPLES "/Users/vincen/Vincen/code/trx_addr/CMakeLists.txt;166;add_test;/Users/vincen/Vincen/code/trx_addr/CMakeLists.txt;0;")
add_test(cli_cpu_smoke "/Users/vincen/Vincen/code/trx_addr/build_test/trx_vanity" "contains" "T" "--max-attempts" "64" "-t" "1")
set_tests_properties(cli_cpu_smoke PROPERTIES  TIMEOUT "10" _BACKTRACE_TRIPLES "/Users/vincen/Vincen/code/trx_addr/CMakeLists.txt;187;add_test;/Users/vincen/Vincen/code/trx_addr/CMakeLists.txt;0;")
add_test(cli_cpu_suffix_smoke "/Users/vincen/Vincen/code/trx_addr/build_test/trx_vanity" "suffix" "8888" "--max-attempts" "64" "-t" "1")
set_tests_properties(cli_cpu_suffix_smoke PROPERTIES  TIMEOUT "10" _BACKTRACE_TRIPLES "/Users/vincen/Vincen/code/trx_addr/CMakeLists.txt;191;add_test;/Users/vincen/Vincen/code/trx_addr/CMakeLists.txt;0;")
add_test(cli_cpu_prefix_smoke "/Users/vincen/Vincen/code/trx_addr/build_test/trx_vanity" "prefix" "ABC" "--max-attempts" "64" "-t" "1")
set_tests_properties(cli_cpu_prefix_smoke PROPERTIES  TIMEOUT "10" _BACKTRACE_TRIPLES "/Users/vincen/Vincen/code/trx_addr/CMakeLists.txt;195;add_test;/Users/vincen/Vincen/code/trx_addr/CMakeLists.txt;0;")
add_test(cli_cpu_consecutive_smoke "/Users/vincen/Vincen/code/trx_addr/build_test/trx_vanity" "consecutive" "8" "4" "--max-attempts" "64" "-t" "1")
set_tests_properties(cli_cpu_consecutive_smoke PROPERTIES  TIMEOUT "10" _BACKTRACE_TRIPLES "/Users/vincen/Vincen/code/trx_addr/CMakeLists.txt;199;add_test;/Users/vincen/Vincen/code/trx_addr/CMakeLists.txt;0;")
add_test(cli_cpu_sequential_smoke "/Users/vincen/Vincen/code/trx_addr/build_test/trx_vanity" "sequential" "1" "4" "--max-attempts" "64" "-t" "1")
set_tests_properties(cli_cpu_sequential_smoke PROPERTIES  TIMEOUT "10" _BACKTRACE_TRIPLES "/Users/vincen/Vincen/code/trx_addr/CMakeLists.txt;203;add_test;/Users/vincen/Vincen/code/trx_addr/CMakeLists.txt;0;")
add_test(cli_private_key_output_security "/opt/homebrew/bin/cmake" "-P" "/Users/vincen/Vincen/code/trx_addr/build_test/cli_private_key_output.cmake")
set_tests_properties(cli_private_key_output_security PROPERTIES  TIMEOUT "20" _BACKTRACE_TRIPLES "/Users/vincen/Vincen/code/trx_addr/CMakeLists.txt;211;add_test;/Users/vincen/Vincen/code/trx_addr/CMakeLists.txt;0;")
add_test(cli_argument_validation "/opt/homebrew/bin/cmake" "-P" "/Users/vincen/Vincen/code/trx_addr/build_test/cli_argument_validation.cmake")
set_tests_properties(cli_argument_validation PROPERTIES  TIMEOUT "10" _BACKTRACE_TRIPLES "/Users/vincen/Vincen/code/trx_addr/CMakeLists.txt;219;add_test;/Users/vincen/Vincen/code/trx_addr/CMakeLists.txt;0;")
add_test(install_package_content_smoke "/opt/homebrew/bin/cmake" "-P" "/Users/vincen/Vincen/code/trx_addr/build_test/install_content_smoke.cmake")
set_tests_properties(install_package_content_smoke PROPERTIES  TIMEOUT "30" _BACKTRACE_TRIPLES "/Users/vincen/Vincen/code/trx_addr/CMakeLists.txt;227;add_test;/Users/vincen/Vincen/code/trx_addr/CMakeLists.txt;0;")
add_test(macos_packaging_files_smoke "/opt/homebrew/bin/cmake" "-P" "/Users/vincen/Vincen/code/trx_addr/build_test/macos_packaging_files_smoke.cmake")
set_tests_properties(macos_packaging_files_smoke PROPERTIES  TIMEOUT "10" _BACKTRACE_TRIPLES "/Users/vincen/Vincen/code/trx_addr/CMakeLists.txt;235;add_test;/Users/vincen/Vincen/code/trx_addr/CMakeLists.txt;0;")

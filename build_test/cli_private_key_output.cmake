set(TRX_VANITY "/Users/vincen/Vincen/code/trx_addr/build_test/trx_vanity")
set(TEST_OUTPUT_DIR "/Users/vincen/Vincen/code/trx_addr/build_test/cli-private-key-output")
file(REMOVE_RECURSE "${TEST_OUTPUT_DIR}")
file(MAKE_DIRECTORY "${TEST_OUTPUT_DIR}")

function(run_cli CASE_NAME OUT_VAR)
    execute_process(
        COMMAND "${TRX_VANITY}" ${ARGN}
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE error
        TIMEOUT 10
    )
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "${CASE_NAME}: trx_vanity exited with ${result}; stderr is suppressed to avoid leaking secrets")
    endif()
    set(${OUT_VAR} "${output}\n${error}" PARENT_SCOPE)
endfunction()

function(run_cli_expect_failure CASE_NAME OUT_VAR)
    execute_process(
        COMMAND "${TRX_VANITY}" ${ARGN}
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE error
        TIMEOUT 10
    )
    if(result EQUAL 0)
        message(FATAL_ERROR "${CASE_NAME}: expected failure but command succeeded")
    endif()
    set(${OUT_VAR} "${output}\n${error}" PARENT_SCOPE)
endfunction()

function(run_cli_with_env CASE_NAME OUT_VAR ENV_ASSIGNMENT)
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E env "${ENV_ASSIGNMENT}" "${TRX_VANITY}" ${ARGN}
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE error
        TIMEOUT 10
    )
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "${CASE_NAME}: trx_vanity exited with ${result}; stderr is suppressed to avoid leaking secrets")
    endif()
    set(${OUT_VAR} "${output}\n${error}" PARENT_SCOPE)
endfunction()

function(assert_not_contains_secret CASE_NAME TEXT)
    if(TEXT MATCHES "Private Key")
        message(FATAL_ERROR "${CASE_NAME}: default output must not contain the Private Key label")
    endif()
    if(TEXT MATCHES "(^|[^0-9A-Fa-f])[0-9A-Fa-f]{64}([^0-9A-Fa-f]|$)")
        message(FATAL_ERROR "${CASE_NAME}: default output must not contain a 64-hex private key")
    endif()
endfunction()

function(assert_csv_second_field_is_not_private_key CASE_NAME TEXT)
    string(STRIP "${TEXT}" stripped)
    string(REPLACE "," ";" fields "${stripped}")
    list(LENGTH fields field_count)
    if(field_count GREATER 1)
        list(GET fields 1 second_field)
        string(LENGTH "${second_field}" second_field_len)
        if(second_field_len EQUAL 64 AND second_field MATCHES "^[0-9a-fA-F]+$")
            message(FATAL_ERROR "${CASE_NAME}: default CSV second field must not be a private key")
        endif()
    endif()
endfunction()

function(assert_csv_second_field_is_private_key CASE_NAME TEXT)
    string(STRIP "${TEXT}" stripped)
    string(REPLACE "," ";" fields "${stripped}")
    list(LENGTH fields field_count)
    if(field_count LESS 2)
        message(FATAL_ERROR "${CASE_NAME}: expected CSV field containing private key")
    endif()
    list(GET fields 1 second_field)
    string(LENGTH "${second_field}" second_field_len)
    if(NOT second_field_len EQUAL 64 OR NOT second_field MATCHES "^[0-9a-fA-F]+$")
        message(FATAL_ERROR "${CASE_NAME}: expected private key only with explicit export permission")
    endif()
endfunction()

run_cli("default stdout" default_output contains T --max-attempts 1 -t 1)
assert_not_contains_secret("default stdout" "${default_output}")
if(NOT default_output MATCHES "Address:")
    message(FATAL_ERROR "default stdout: expected address in match output")
endif()
if(NOT default_output MATCHES "Security:")
    message(FATAL_ERROR "default stdout: expected security hint in match output")
endif()

run_cli("explicit stdout" explicit_output contains T --max-attempts 1 -t 1 --show-private-key)
if(NOT explicit_output MATCHES "WARNING")
    message(FATAL_ERROR "explicit stdout: expected strong warning when showing private key")
endif()
if(NOT explicit_output MATCHES "Private Key")
    message(FATAL_ERROR "explicit stdout: expected private key label only with --show-private-key")
endif()

set(default_csv "${TEST_OUTPUT_DIR}/default.csv")
file(REMOVE "${default_csv}")
run_cli("default plaintext export" default_export contains T --max-attempts 1 -t 1 -o "${default_csv}")
file(READ "${default_csv}" default_csv_content)
assert_not_contains_secret("default plaintext export" "${default_csv_content}")
assert_csv_second_field_is_not_private_key("default plaintext export" "${default_csv_content}")

set(rejected_csv "${TEST_OUTPUT_DIR}/rejected.csv")
file(REMOVE "${rejected_csv}")
run_cli_expect_failure("rejected plaintext private-key export" rejected_export contains T --max-attempts 1 -t 1 -o "${rejected_csv}" --allow-plaintext-private-key-output)
if(NOT rejected_export MATCHES "plaintext private-key file export is disabled")
    message(FATAL_ERROR "rejected plaintext private-key export: expected clear safety error")
endif()
if(EXISTS "${rejected_csv}")
    file(READ "${rejected_csv}" rejected_csv_content)
    assert_not_contains_secret("rejected plaintext private-key export" "${rejected_csv_content}")
endif()

set(encrypted_export "${TEST_OUTPUT_DIR}/encrypted.trxenc")
file(REMOVE "${encrypted_export}")
set(export_password "test-password-do-not-log")
run_cli("encrypted private-key export" encrypted_output contains T --max-attempts 1 -t 1 --encrypted-output "${encrypted_export}" --export-password "${export_password}")
if(encrypted_output MATCHES "${export_password}")
    message(FATAL_ERROR "encrypted private-key export: password leaked to stdout/stderr")
endif()
assert_not_contains_secret("encrypted private-key export logs" "${encrypted_output}")
file(READ "${encrypted_export}" encrypted_content)
if(NOT encrypted_content MATCHES "TRX-EXPORT-AES-256-GCM-v1")
    message(FATAL_ERROR "encrypted private-key export: expected encrypted export marker")
endif()
if(encrypted_content MATCHES "${export_password}")
    message(FATAL_ERROR "encrypted private-key export: password leaked to file")
endif()
assert_not_contains_secret("encrypted private-key export file" "${encrypted_content}")

set(env_encrypted_export "${TEST_OUTPUT_DIR}/env-encrypted.trxenc")
file(REMOVE "${env_encrypted_export}")
set(env_export_password "test-env-password-do-not-log")
run_cli_with_env("encrypted private-key export via env" env_encrypted_output "TRX_TEST_EXPORT_PASSWORD=${env_export_password}" contains T --max-attempts 1 -t 1 --encrypted-output "${env_encrypted_export}" --export-password-env TRX_TEST_EXPORT_PASSWORD)
if(env_encrypted_output MATCHES "${env_export_password}")
    message(FATAL_ERROR "encrypted private-key export via env: password leaked to stdout/stderr")
endif()
assert_not_contains_secret("encrypted private-key export via env logs" "${env_encrypted_output}")
file(READ "${env_encrypted_export}" env_encrypted_content)
if(NOT env_encrypted_content MATCHES "TRX-EXPORT-AES-256-GCM-v1")
    message(FATAL_ERROR "encrypted private-key export via env: expected encrypted export marker")
endif()
if(env_encrypted_content MATCHES "${env_export_password}")
    message(FATAL_ERROR "encrypted private-key export via env: password leaked to file")
endif()
assert_not_contains_secret("encrypted private-key export via env file" "${env_encrypted_content}")

run_cli_expect_failure("encrypted export missing password" missing_password_output contains T --max-attempts 1 -t 1 --encrypted-output "${TEST_OUTPUT_DIR}/missing.trxenc")
if(NOT missing_password_output MATCHES "requires an export password")
    message(FATAL_ERROR "encrypted export missing password: expected clear error")
endif()

run_cli_expect_failure("encrypted export missing env password" missing_env_password_output contains T --max-attempts 1 -t 1 --encrypted-output "${TEST_OUTPUT_DIR}/missing-env.trxenc" --export-password-env TRX_TEST_EXPORT_PASSWORD_NOT_SET)
if(NOT missing_env_password_output MATCHES "environment variable for --export-password-env is not set or is empty")
    message(FATAL_ERROR "encrypted export missing env password: expected clear error")
endif()

run_cli_expect_failure("encrypted export conflicting password sources" conflicting_password_output contains T --max-attempts 1 -t 1 --encrypted-output "${TEST_OUTPUT_DIR}/conflict.trxenc" --export-password-env TRX_TEST_EXPORT_PASSWORD --export-password "test-password-do-not-log")
if(NOT conflicting_password_output MATCHES "use only one export password source")
    message(FATAL_ERROR "encrypted export conflicting password sources: expected clear error")
endif()

file(REMOVE_RECURSE "${TEST_OUTPUT_DIR}")

set(PROJECT_SOURCE_DIR "/Users/vincen/Vincen/code/trx_addr")

set(required_files
    "packaging/macos/Info.plist.in"
    "packaging/macos/README_zh.md"
    "packaging/macos/sign_and_notarize_zh.md"
)

foreach(relative_path IN LISTS required_files)
    if(NOT EXISTS "${PROJECT_SOURCE_DIR}/${relative_path}")
        message(FATAL_ERROR "Missing macOS packaging file: ${relative_path}")
    endif()
endforeach()

file(READ "${PROJECT_SOURCE_DIR}/packaging/macos/README_zh.md" macos_readme)
foreach(required_text IN ITEMS ".app" ".dmg" "Gatekeeper" "TRX_KERNEL_DIR" "私钥")
    string(FIND "${macos_readme}" "${required_text}" found_index)
    if(found_index EQUAL -1)
        message(FATAL_ERROR "macOS README is missing required text: ${required_text}")
    endif()
endforeach()

file(READ "${PROJECT_SOURCE_DIR}/packaging/macos/sign_and_notarize_zh.md" notarize_doc)
foreach(required_text IN ITEMS "codesign" "notarytool" "stapler" "spctl" "SHA256")
    string(FIND "${notarize_doc}" "${required_text}" found_index)
    if(found_index EQUAL -1)
        message(FATAL_ERROR "sign/notarize doc is missing required text: ${required_text}")
    endif()
endforeach()

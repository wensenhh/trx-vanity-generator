#include "utils/first_run.h"
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <unistd.h>

namespace trx {

std::string FirstRunWizard::marker_path() {
    const char* home = std::getenv("HOME");
    if (!home || home[0] == '\0') {
        home = ".";
    }
    return std::string(home) + "/.trx_vanity_first_run";
}

bool FirstRunWizard::already_acknowledged() {
    std::ifstream ifs(marker_path());
    return ifs.is_open();
}

bool FirstRunWizard::write_acknowledgment() {
    std::ofstream ofs(marker_path());
    if (!ofs.is_open()) return false;
    ofs << "acknowledged\n";
    return ofs.good();
}

bool FirstRunWizard::check_and_show() {
    if (already_acknowledged()) {
        return true;
    }

    // Non-interactive / CI safe: if stdin is not a TTY, just print a reminder
    bool is_tty = isatty(STDIN_FILENO) && isatty(STDOUT_FILENO);

    std::cout << R"(
================================================================================
                          TRX Vanity Generator - Security Notice
================================================================================

  PRIVATE KEYS CONTROL YOUR ASSETS. Anyone with your private key can spend
  all TRX, USDT-TRC20, and any TRC20 tokens in the matching address.

  DEFAULT SAFE BEHAVIOR:
    - Private keys are HIDDEN in terminal output.
    - Plaintext output files do NOT contain private keys.
    - Use --encrypted-output with --export-password-env to save keys safely.

  HIGH-RISK OPTIONS (use only if you understand the risk):
    --show-private-key          Prints private keys to the terminal
    --export-password <pwd>     Password may leak into shell history

  NEVER:
    - Send private keys via Telegram, WeChat, email, or cloud storage
    - Screenshot or screen-share while private keys are visible
    - Run this tool on shared, public, or remote-access computers

================================================================================
)";

    if (!is_tty) {
        std::cout << "\n[Non-interactive mode] Continuing. Run 'trx_vanity --security-notice' to review.\n\n";
        write_acknowledgment();
        return true;
    }

    std::cout << "Do you understand and accept these security terms? [y/N]: ";
    std::string response;
    if (!std::getline(std::cin, response)) {
        return false;
    }
    if (response.empty() || (response[0] != 'y' && response[0] != 'Y')) {
        std::cout << "\nAborted. You must acknowledge the security notice to use this tool.\n";
        std::cout << "Run again and type 'y' to continue, or read docs/SECURITY_zh.md.\n";
        return false;
    }

    write_acknowledgment();
    std::cout << "\n";
    return true;
}

bool FirstRunWizard::reset_acknowledgment() {
    return std::remove(marker_path().c_str()) == 0;
}

} // namespace trx

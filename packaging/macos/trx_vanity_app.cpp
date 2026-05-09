#include <mach-o/dyld.h>
#include <sys/stat.h>
#include <unistd.h>

#include <array>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>

namespace {

bool exists(const std::string& path) {
    struct stat st {};
    return ::stat(path.c_str(), &st) == 0;
}

std::string shell_quote(const std::string& value) {
    std::string out = "'";
    for (char c : value) {
        if (c == '\'') {
            out += "'\\''";
        } else {
            out += c;
        }
    }
    out += "'";
    return out;
}

std::string applescript_quote(const std::string& value) {
    std::string out = "\"";
    for (char c : value) {
        if (c == '\\' || c == '"') {
            out += '\\';
        }
        out += c;
    }
    out += "\"";
    return out;
}

std::string dirname(std::string path) {
    const auto pos = path.find_last_of('/');
    if (pos == std::string::npos) return ".";
    if (pos == 0) return "/";
    return path.substr(0, pos);
}

std::string executable_path() {
    std::array<char, 4096> buffer {};
    uint32_t size = static_cast<uint32_t>(buffer.size());
    if (_NSGetExecutablePath(buffer.data(), &size) != 0) {
        return {};
    }
    return buffer.data();
}

}  // namespace

int main() {
    const std::string exe = executable_path();
    if (exe.empty()) {
        std::cerr << "Unable to resolve app bundle path.\n";
        return 1;
    }

    const std::string macos_dir = dirname(exe);
    const std::string contents_dir = dirname(macos_dir);
    const std::string resources_dir = contents_dir + "/Resources";
    const std::string bundled_cli = resources_dir + "/bin/trx_vanity";
    const std::string kernel_dir = resources_dir + "/kernel";
    const std::string readme = resources_dir + "/README_zh.md";

    if (!exists(bundled_cli)) {
        std::cerr << "Bundled CLI is missing: " << bundled_cli << "\n";
        return 1;
    }

    std::ostringstream terminal_script;
    terminal_script
        << "clear; "
        << "echo 'TRX Vanity macOS app launcher'; "
        << "echo 'Private keys are hidden unless you explicitly pass --show-private-key.'; "
        << "export TRX_KERNEL_DIR=" << shell_quote(kernel_dir) << "; "
        << shell_quote(bundled_cli) << " --help; "
        << "echo; echo 'Docs: ' " << shell_quote(readme) << "; "
        << "echo 'Example CPU smoke: ' " << shell_quote(bundled_cli) << " ' suffix 8888 --max-attempts 64 -t 1'; "
        << "echo 'Close this Terminal window when finished.'";

    std::ostringstream command;
    command << "/usr/bin/osascript -e "
            << shell_quote("tell application \"Terminal\" to do script " + applescript_quote(terminal_script.str()))
            << " -e " << shell_quote("tell application \"Terminal\" to activate");

    const int rc = std::system(command.str().c_str());
    if (rc != 0) {
        std::cerr << "Failed to open Terminal launcher. Command exit: " << rc << "\n";
        return 1;
    }
    return 0;
}

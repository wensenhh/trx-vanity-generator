#include "utils/export_encryption.h"

#include <iostream>
#include <regex>
#include <string>

int main() {
    const std::string password = "correct horse battery staple";
    const std::string wrong_password = "wrong password";
    const std::string plaintext =
        "TExampleAddressForUnitTest,0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef,contains T,1";

    const std::string encrypted = trx::encrypt_export_record(plaintext, password);
    if (!trx::looks_like_encrypted_export_record(encrypted)) {
        std::cerr << "encrypted record has unexpected format\n";
        return 1;
    }
    if (encrypted.find("0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef") != std::string::npos) {
        std::cerr << "encrypted record leaked private key plaintext\n";
        return 1;
    }
    if (encrypted.find(password) != std::string::npos) {
        std::cerr << "encrypted record leaked password plaintext\n";
        return 1;
    }

    std::string decrypted;
    if (!trx::decrypt_export_record(encrypted, password, decrypted) || decrypted != plaintext) {
        std::cerr << "correct password failed to decrypt\n";
        return 1;
    }
    if (trx::decrypt_export_record(encrypted, wrong_password, decrypted)) {
        std::cerr << "wrong password unexpectedly decrypted\n";
        return 1;
    }
    if (trx::decrypt_export_record(encrypted.substr(0, encrypted.size() - 1) + "A", password, decrypted)) {
        std::cerr << "tampered record unexpectedly decrypted\n";
        return 1;
    }

    return 0;
}

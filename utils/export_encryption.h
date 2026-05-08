#ifndef TRX_EXPORT_ENCRYPTION_H
#define TRX_EXPORT_ENCRYPTION_H

#include <cstdint>
#include <string>
#include <vector>

namespace trx {

struct EncryptedExportRecord {
    static constexpr int kPbkdf2Iterations = 210000;

    std::vector<uint8_t> salt;
    std::vector<uint8_t> nonce;
    std::vector<uint8_t> ciphertext;
    std::vector<uint8_t> tag;
};

// Encrypts a single export record using AES-256-GCM with a key derived from
// the passphrase via PBKDF2-HMAC-SHA256. The returned text is safe to append as
// one line to an export file and intentionally contains no plaintext fields.
std::string encrypt_export_record(const std::string& plaintext,
                                  const std::string& passphrase);

// Decrypts one line produced by encrypt_export_record. Returns false for wrong
// passwords, tampering, unsupported format, or malformed input.
bool decrypt_export_record(const std::string& encoded_record,
                           const std::string& passphrase,
                           std::string& plaintext_out);

bool looks_like_encrypted_export_record(const std::string& encoded_record);

} // namespace trx

#endif // TRX_EXPORT_ENCRYPTION_H

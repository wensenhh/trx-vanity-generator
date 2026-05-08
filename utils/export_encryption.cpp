#include "utils/export_encryption.h"

#include <openssl/evp.h>
#include <openssl/rand.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace trx {
namespace {

constexpr char kPrefix[] = "TRX-EXPORT-AES-256-GCM-v1";
constexpr size_t kSaltLen = 16;
constexpr size_t kNonceLen = 12;
constexpr size_t kKeyLen = 32;
constexpr size_t kTagLen = 16;

std::vector<uint8_t> secure_random(size_t len) {
    std::vector<uint8_t> out(len);
    if (RAND_bytes(out.data(), static_cast<int>(out.size())) != 1) {
        throw std::runtime_error("OpenSSL RAND_bytes failed");
    }
    return out;
}

std::array<uint8_t, kKeyLen> derive_key(const std::string& passphrase,
                                        const std::vector<uint8_t>& salt) {
    if (passphrase.empty()) {
        throw std::invalid_argument("export passphrase must not be empty");
    }
    std::array<uint8_t, kKeyLen> key{};
    if (PKCS5_PBKDF2_HMAC(passphrase.data(), static_cast<int>(passphrase.size()),
                          salt.data(), static_cast<int>(salt.size()),
                          EncryptedExportRecord::kPbkdf2Iterations,
                          EVP_sha256(), static_cast<int>(key.size()),
                          key.data()) != 1) {
        throw std::runtime_error("OpenSSL PBKDF2 failed");
    }
    return key;
}

std::string b64_encode(const std::vector<uint8_t>& bytes) {
    if (bytes.empty()) return "";
    std::string out(4 * ((bytes.size() + 2) / 3), '\0');
    int len = EVP_EncodeBlock(reinterpret_cast<unsigned char*>(&out[0]),
                              bytes.data(), static_cast<int>(bytes.size()));
    if (len < 0) {
        throw std::runtime_error("OpenSSL base64 encode failed");
    }
    out.resize(static_cast<size_t>(len));
    return out;
}

bool b64_decode(const std::string& text, std::vector<uint8_t>& out) {
    if (text.empty() || text.size() % 4 != 0) return false;
    out.assign((text.size() / 4) * 3, 0);
    int len = EVP_DecodeBlock(out.data(),
                              reinterpret_cast<const unsigned char*>(text.data()),
                              static_cast<int>(text.size()));
    if (len < 0) return false;
    size_t padding = 0;
    if (!text.empty() && text[text.size() - 1] == '=') ++padding;
    if (text.size() > 1 && text[text.size() - 2] == '=') ++padding;
    if (static_cast<size_t>(len) < padding) return false;
    out.resize(static_cast<size_t>(len) - padding);
    return true;
}

bool split_record(const std::string& record,
                  std::vector<std::string>& parts) {
    parts.clear();
    size_t start = 0;
    while (true) {
        size_t pos = record.find(':', start);
        if (pos == std::string::npos) {
            parts.push_back(record.substr(start));
            break;
        }
        parts.push_back(record.substr(start, pos - start));
        start = pos + 1;
    }
    return parts.size() == 5 && parts[0] == kPrefix;
}

} // namespace

std::string encrypt_export_record(const std::string& plaintext,
                                  const std::string& passphrase) {
    auto salt = secure_random(kSaltLen);
    auto nonce = secure_random(kNonceLen);
    auto key = derive_key(passphrase, salt);

    std::vector<uint8_t> ciphertext(plaintext.size() + EVP_MAX_BLOCK_LENGTH);
    std::vector<uint8_t> tag(kTagLen);
    int out_len = 0;
    int total_len = 0;

    EVP_CIPHER_CTX* raw_ctx = EVP_CIPHER_CTX_new();
    if (!raw_ctx) {
        throw std::runtime_error("OpenSSL EVP_CIPHER_CTX_new failed");
    }

    int ok = EVP_EncryptInit_ex(raw_ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr);
    ok = ok && EVP_CIPHER_CTX_ctrl(raw_ctx, EVP_CTRL_GCM_SET_IVLEN,
                                   static_cast<int>(nonce.size()), nullptr);
    ok = ok && EVP_EncryptInit_ex(raw_ctx, nullptr, nullptr, key.data(), nonce.data());
    ok = ok && EVP_EncryptUpdate(raw_ctx, ciphertext.data(), &out_len,
                                 reinterpret_cast<const unsigned char*>(plaintext.data()),
                                 static_cast<int>(plaintext.size()));
    total_len = out_len;
    ok = ok && EVP_EncryptFinal_ex(raw_ctx, ciphertext.data() + total_len, &out_len);
    total_len += out_len;
    ok = ok && EVP_CIPHER_CTX_ctrl(raw_ctx, EVP_CTRL_GCM_GET_TAG,
                                   static_cast<int>(tag.size()), tag.data());
    EVP_CIPHER_CTX_free(raw_ctx);

    std::fill(key.begin(), key.end(), 0);
    if (!ok) {
        throw std::runtime_error("OpenSSL AES-GCM encryption failed");
    }
    ciphertext.resize(static_cast<size_t>(total_len));

    return std::string(kPrefix) + ":" + b64_encode(salt) + ":" +
           b64_encode(nonce) + ":" + b64_encode(ciphertext) + ":" +
           b64_encode(tag);
}

bool decrypt_export_record(const std::string& encoded_record,
                           const std::string& passphrase,
                           std::string& plaintext_out) {
    plaintext_out.clear();
    std::vector<std::string> parts;
    if (!split_record(encoded_record, parts)) return false;

    std::vector<uint8_t> salt;
    std::vector<uint8_t> nonce;
    std::vector<uint8_t> ciphertext;
    std::vector<uint8_t> tag;
    if (!b64_decode(parts[1], salt) || !b64_decode(parts[2], nonce) ||
        !b64_decode(parts[3], ciphertext) || !b64_decode(parts[4], tag)) {
        return false;
    }
    if (salt.size() != kSaltLen || nonce.size() != kNonceLen || tag.size() != kTagLen) {
        return false;
    }

    std::array<uint8_t, kKeyLen> key{};
    try {
        key = derive_key(passphrase, salt);
    } catch (...) {
        return false;
    }

    std::vector<uint8_t> plaintext(ciphertext.size() + EVP_MAX_BLOCK_LENGTH);
    int out_len = 0;
    int total_len = 0;

    EVP_CIPHER_CTX* raw_ctx = EVP_CIPHER_CTX_new();
    if (!raw_ctx) return false;

    int ok = EVP_DecryptInit_ex(raw_ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr);
    ok = ok && EVP_CIPHER_CTX_ctrl(raw_ctx, EVP_CTRL_GCM_SET_IVLEN,
                                   static_cast<int>(nonce.size()), nullptr);
    ok = ok && EVP_DecryptInit_ex(raw_ctx, nullptr, nullptr, key.data(), nonce.data());
    ok = ok && EVP_DecryptUpdate(raw_ctx, plaintext.data(), &out_len,
                                 ciphertext.data(), static_cast<int>(ciphertext.size()));
    total_len = out_len;
    ok = ok && EVP_CIPHER_CTX_ctrl(raw_ctx, EVP_CTRL_GCM_SET_TAG,
                                   static_cast<int>(tag.size()), tag.data());
    ok = ok && EVP_DecryptFinal_ex(raw_ctx, plaintext.data() + total_len, &out_len);
    total_len += out_len;
    EVP_CIPHER_CTX_free(raw_ctx);

    std::fill(key.begin(), key.end(), 0);
    if (!ok) return false;
    plaintext_out.assign(reinterpret_cast<const char*>(plaintext.data()),
                         static_cast<size_t>(total_len));
    return true;
}

bool looks_like_encrypted_export_record(const std::string& encoded_record) {
    return encoded_record.rfind(kPrefix, 0) == 0;
}

} // namespace trx

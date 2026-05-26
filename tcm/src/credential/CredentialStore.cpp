#include "credential/CredentialStore.hpp"

#include <nlohmann/json.hpp>
#include <openssl/evp.h>
#include <openssl/rand.h>

#include <cstring>
#include <fstream>
#include <stdexcept>

namespace tcm {

static constexpr uint8_t MAGIC[4] = {'T', 'C', 'M', 'C'};
static constexpr int SALT_LEN     = 16;
static constexpr int NONCE_LEN    = 12;
static constexpr int TAG_LEN      = 16;
static constexpr int KEY_LEN      = 32;
static constexpr int PBKDF2_ITER  = 100000;

// ---------------------------------------------------------------------------
CredentialStore::CredentialStore(const std::string& path,
                                 const std::string& passphrase)
    : m_path(path), m_passphrase(passphrase) {}

// ---------------------------------------------------------------------------
bool CredentialStore::store(const std::string& id, const std::string& secret) {
    m_data[id] = secret;
    return true;
}

// ---------------------------------------------------------------------------
std::optional<std::string> CredentialStore::retrieve(
    const std::string& id) const {
    auto it = m_data.find(id);
    if (it != m_data.end()) return it->second;
    return std::nullopt;
}

// ---------------------------------------------------------------------------
bool CredentialStore::remove(const std::string& id) {
    return m_data.erase(id) > 0;
}

// ---------------------------------------------------------------------------
bool CredentialStore::contains(const std::string& id) const {
    return m_data.count(id) > 0;
}

// ---------------------------------------------------------------------------
std::vector<std::string> CredentialStore::listIds() const {
    std::vector<std::string> ids;
    ids.reserve(m_data.size());
    for (const auto& [k, v] : m_data) ids.push_back(k);
    return ids;
}

// ---------------------------------------------------------------------------
std::vector<uint8_t> CredentialStore::deriveKey(
    const std::vector<uint8_t>& salt) const {
    std::vector<uint8_t> key(KEY_LEN);
    PKCS5_PBKDF2_HMAC(m_passphrase.c_str(),
                      static_cast<int>(m_passphrase.size()), salt.data(),
                      static_cast<int>(salt.size()), PBKDF2_ITER, EVP_sha256(),
                      KEY_LEN, key.data());
    return key;
}

// ---------------------------------------------------------------------------
std::vector<uint8_t> CredentialStore::encrypt(
    const std::string& plaintext) const {
    // Random salt and nonce
    std::vector<uint8_t> salt(SALT_LEN);
    std::vector<uint8_t> nonce(NONCE_LEN);
    RAND_bytes(salt.data(), SALT_LEN);
    RAND_bytes(nonce.data(), NONCE_LEN);

    auto key = deriveKey(salt);

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr);
    EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, NONCE_LEN, nullptr);
    EVP_EncryptInit_ex(ctx, nullptr, nullptr, key.data(), nonce.data());

    std::vector<uint8_t> ciphertext(plaintext.size());
    int outlen = 0;
    EVP_EncryptUpdate(ctx, ciphertext.data(), &outlen,
                      reinterpret_cast<const uint8_t*>(plaintext.data()),
                      static_cast<int>(plaintext.size()));
    int finalLen = 0;
    EVP_EncryptFinal_ex(ctx, ciphertext.data() + outlen, &finalLen);
    ciphertext.resize(outlen + finalLen);

    std::vector<uint8_t> tag(TAG_LEN);
    EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, TAG_LEN, tag.data());
    EVP_CIPHER_CTX_free(ctx);

    // File format: [4-byte magic] + [16-byte salt] + [12-byte nonce]
    //              + [16-byte auth tag] + [ciphertext]
    std::vector<uint8_t> result;
    result.reserve(4 + SALT_LEN + NONCE_LEN + TAG_LEN + ciphertext.size());
    result.insert(result.end(), MAGIC, MAGIC + 4);
    result.insert(result.end(), salt.begin(), salt.end());
    result.insert(result.end(), nonce.begin(), nonce.end());
    result.insert(result.end(), tag.begin(), tag.end());
    result.insert(result.end(), ciphertext.begin(), ciphertext.end());
    return result;
}

// ---------------------------------------------------------------------------
std::optional<std::string> CredentialStore::decrypt(
    const std::vector<uint8_t>& data) const {
    static constexpr size_t HEADER_SIZE = 4 + SALT_LEN + NONCE_LEN + TAG_LEN;
    if (data.size() < HEADER_SIZE) return std::nullopt;

    // Verify magic
    if (std::memcmp(data.data(), MAGIC, 4) != 0) return std::nullopt;

    size_t offset = 4;
    std::vector<uint8_t> salt(data.begin() + offset,
                               data.begin() + offset + SALT_LEN);
    offset += SALT_LEN;
    std::vector<uint8_t> nonce(data.begin() + offset,
                                data.begin() + offset + NONCE_LEN);
    offset += NONCE_LEN;
    std::vector<uint8_t> tag(data.begin() + offset,
                              data.begin() + offset + TAG_LEN);
    offset += TAG_LEN;
    std::vector<uint8_t> ciphertext(data.begin() + offset, data.end());

    auto key = deriveKey(salt);

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr);
    EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, NONCE_LEN, nullptr);
    EVP_DecryptInit_ex(ctx, nullptr, nullptr, key.data(), nonce.data());

    std::vector<uint8_t> plaintext(ciphertext.size());
    int outlen = 0;
    EVP_DecryptUpdate(ctx, plaintext.data(), &outlen, ciphertext.data(),
                      static_cast<int>(ciphertext.size()));

    // Set the expected tag before calling Final
    EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, TAG_LEN,
                        const_cast<uint8_t*>(tag.data()));
    int finalLen = 0;
    int ret = EVP_DecryptFinal_ex(ctx, plaintext.data() + outlen, &finalLen);
    EVP_CIPHER_CTX_free(ctx);

    if (ret <= 0) return std::nullopt;  // Auth tag verification failed

    plaintext.resize(outlen + finalLen);
    return std::string(plaintext.begin(), plaintext.end());
}

// ---------------------------------------------------------------------------
bool CredentialStore::save() const {
    if (m_path.empty()) return false;

    nlohmann::json j(m_data);
    std::string jsonStr = j.dump();

    auto encrypted = encrypt(jsonStr);

    std::ofstream file(m_path, std::ios::binary | std::ios::trunc);
    if (!file) return false;

    file.write(reinterpret_cast<const char*>(encrypted.data()),
               static_cast<std::streamsize>(encrypted.size()));
    return file.good();
}

// ---------------------------------------------------------------------------
bool CredentialStore::load() {
    if (m_path.empty()) return false;

    std::ifstream file(m_path, std::ios::binary);
    if (!file) return false;

    std::vector<uint8_t> data((std::istreambuf_iterator<char>(file)),
                               std::istreambuf_iterator<char>());

    auto decrypted = decrypt(data);
    if (!decrypted) return false;

    try {
        auto j  = nlohmann::json::parse(*decrypted);
        m_data  = j.get<std::map<std::string, std::string>>();
    } catch (...) {
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
void CredentialStore::clear() { m_data.clear(); }

}  // namespace tcm

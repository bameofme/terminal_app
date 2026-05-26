#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace tcm {

class CredentialStore {
public:
    // path: path to encrypted credential file
    // passphrase: encryption key (derived from passphrase via PBKDF2)
    explicit CredentialStore(const std::string& path = "",
                             const std::string& passphrase = "");

    // Store a credential
    bool store(const std::string& id, const std::string& secret);

    // Retrieve a credential
    std::optional<std::string> retrieve(const std::string& id) const;

    // Remove a credential
    bool remove(const std::string& id);

    // Check if credential exists
    bool contains(const std::string& id) const;

    // List all credential IDs
    std::vector<std::string> listIds() const;

    // Save to disk (encrypted)
    bool save() const;

    // Load from disk (decrypt)
    bool load();

    // Clear all in-memory credentials (does not delete file)
    void clear();

private:
    std::string m_path;
    std::string m_passphrase;
    std::map<std::string, std::string> m_data;  // id → plaintext secret (in memory)

    // Encrypt data with AES-256-GCM using m_passphrase
    std::vector<uint8_t> encrypt(const std::string& plaintext) const;

    // Decrypt data
    std::optional<std::string> decrypt(const std::vector<uint8_t>& ciphertext) const;

    // Derive key from passphrase using PBKDF2-SHA256
    std::vector<uint8_t> deriveKey(const std::vector<uint8_t>& salt) const;
};

}  // namespace tcm

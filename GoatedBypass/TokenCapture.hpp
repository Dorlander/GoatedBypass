#pragma once
#include <string>
#include <optional>
#include <filesystem>

struct CapturedTokens
{
    std::string playerTokens;   // JSON string of playerTokens
    std::string entitlements;   // Entitlements JWT
    std::string authorization;  // Bearer token
    std::string identity;       // RSO Identity JWT
    std::string timestamp;
};

namespace TokenCapture
{
    // Read lockfile → returns {name, pid, port, password, protocol}
    struct LockfileData
    {
        std::string name;
        uint16_t port;
        std::string password;
        std::string protocol;
    };

    std::optional<LockfileData> readLockfile(const std::filesystem::path& lockPath);

    // Query local API for session/token data
    std::string queryLocalAPI(const LockfileData& lock, const std::string& endpoint);

    // Capture all tokens from running client
    std::optional<CapturedTokens> capture();

    // Save/load tokens to/from file
    bool saveTokens(const CapturedTokens& tokens, const std::filesystem::path& path);
    std::optional<CapturedTokens> loadTokens(const std::filesystem::path& path);
}

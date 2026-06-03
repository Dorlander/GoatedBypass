#include "TokenCapture.hpp"
#include <nlohmann/json.hpp>

#include <fstream>
#include <sstream>
#include <iostream>
#include <ctime>
#include <iomanip>
#include <windows.h>
#include <winhttp.h>

#pragma comment(lib, "winhttp.lib")

using Json = nlohmann::json;

static const char b64chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static std::string base64encode(const std::string& in)
{
    std::string out;
    int val = 0, valb = -6;
    for (unsigned char c : in) {
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0) {
            out.push_back(b64chars[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }
    if (valb > -6) out.push_back(b64chars[((val << 8) >> (valb + 8)) & 0x3F]);
    while (out.size() % 4) out.push_back('=');
    return out;
}
namespace fs = std::filesystem;

namespace
{
    std::string getEnvVar(const char* name)
    {
        if (const char* v = std::getenv(name))
            return std::string(v);
        return "";
    }

    std::string now()
    {
        std::time_t t = std::time(nullptr);
        std::tm tm{};
        localtime_s(&tm, &t);
        std::ostringstream ss;
        ss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S");
        return ss.str();
    }

    // Simple HTTPS GET to localhost with basic auth (ignore cert errors)
    std::string httpsGet(uint16_t port, const std::string& path, const std::string& password)
    {
        HINTERNET hSession = WinHttpOpen(L"TokenCapture/1.0",
            WINHTTP_ACCESS_TYPE_NO_PROXY, nullptr, nullptr, 0);
        if (!hSession) return "";

        HINTERNET hConnect = WinHttpConnect(hSession,
            L"127.0.0.1", port, 0);
        if (!hConnect) { WinHttpCloseHandle(hSession); return ""; }

        std::wstring wpath(path.begin(), path.end());
        HINTERNET hRequest = WinHttpOpenRequest(hConnect,
            L"GET", wpath.c_str(), nullptr, nullptr, nullptr,
            WINHTTP_FLAG_SECURE);
        if (!hRequest) {
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return "";
        }

        // Ignore SSL errors (self-signed cert)
        DWORD flags = SECURITY_FLAG_IGNORE_ALL_CERT_ERRORS;
        WinHttpSetOption(hRequest, WINHTTP_OPTION_SECURITY_FLAGS, &flags, sizeof(flags));

        // Basic auth: riot:<password>
        std::string authStr = "riot:" + password;
        std::string b64 = base64encode(authStr);

        std::wstring authHeader = L"Authorization: Basic ";
        authHeader += std::wstring(b64.begin(), b64.end());
        WinHttpAddRequestHeaders(hRequest, authHeader.c_str(), -1, WINHTTP_ADDREQ_FLAG_ADD);

        if (!WinHttpSendRequest(hRequest, nullptr, 0, nullptr, 0, 0, 0) ||
            !WinHttpReceiveResponse(hRequest, nullptr))
        {
            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return "";
        }

        std::string result;
        DWORD bytesRead = 0;
        char buf[4096];
        while (WinHttpReadData(hRequest, buf, sizeof(buf), &bytesRead) && bytesRead > 0)
        {
            result.append(buf, bytesRead);
        }

        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return result;
    }
}

namespace TokenCapture
{
    std::optional<LockfileData> readLockfile(const fs::path& lockPath)
    {
        if (!fs::exists(lockPath)) return std::nullopt;

        std::ifstream in(lockPath);
        if (!in) return std::nullopt;

        std::string line;
        std::getline(in, line);
        if (line.empty()) return std::nullopt;

        // Format: name:pid:port:password:protocol
        std::vector<std::string> parts;
        std::istringstream ss(line);
        std::string part;
        while (std::getline(ss, part, ':'))
            parts.push_back(part);

        if (parts.size() < 5) return std::nullopt;

        LockfileData data;
        data.name = parts[0];
        data.port = static_cast<uint16_t>(std::stoi(parts[2]));
        data.password = parts[3];
        data.protocol = parts[4];
        return data;
    }

    std::string queryLocalAPI(const LockfileData& lock, const std::string& endpoint)
    {
        return httpsGet(lock.port, endpoint, lock.password);
    }

    std::optional<CapturedTokens> capture()
    {
        // Try Riot Client lockfile first
        std::vector<fs::path> lockPaths = {
            fs::path("C:/Riot Games/League of Legends/lockfile"),
            fs::path("C:/ProgramData/Riot Games/Metadata/league_of_legends.live/league_of_legends.live.lockfile"),
        };

        // Also check RiotClient lockfile in AppData
        std::string localAppData = getEnvVar("LOCALAPPDATA");
        if (!localAppData.empty())
        {
            lockPaths.push_back(fs::path(localAppData) / "Riot Games" / "Riot Client" / "Config" / "lockfile");
        }

        std::optional<LockfileData> lock;
        for (const auto& p : lockPaths)
        {
            lock = readLockfile(p);
            if (lock.has_value())
            {
                std::cout << "[+] Found lockfile: " << p.string() << " port=" << lock->port << std::endl;
                break;
            }
        }

        if (!lock)
        {
            std::cerr << "[!] No lockfile found. Is Riot Client running?" << std::endl;
            return std::nullopt;
        }

        CapturedTokens tokens;
        tokens.timestamp = now();

        // Query various endpoints for token data
        std::vector<std::pair<std::string, std::string>> endpoints = {
            {"/lol-login/v1/session", "login-session"},
            {"/lol-gameflow/v1/session", "gameflow-session"},
            {"/entitlements/v1/token", "entitlements"},
            {"/riotclient/auth-token", "auth-token"},
            {"/riotclient/region-locale", "region"},
            {"/lol-lobby/v2/lobby", "lobby"},
            {"/lol-lobby-team-builder/v1/lobby", "teambuilder-lobby"},
            {"/player-session-lifecycle/v1/session", "psl-session"},
            {"/anti-cheat/v1/session", "anticheat-session"},
            {"/lol-anti-cheat/v1/anti-cheat-session-by-type/vanguard", "vanguard-session"},
        };

        Json allData;
        for (const auto& [ep, name] : endpoints)
        {
            std::string resp = queryLocalAPI(*lock, ep);
            if (!resp.empty())
            {
                std::cout << "[+] " << name << " (" << resp.size() << " bytes)" << std::endl;
                try
                {
                    allData[name] = Json::parse(resp);
                }
                catch (...)
                {
                    allData[name] = resp;
                }
            }
            else
            {
                std::cout << "[-] " << name << " (empty)" << std::endl;
            }
        }

        tokens.playerTokens = allData.dump(2);
        return tokens;
    }

    bool saveTokens(const CapturedTokens& tokens, const fs::path& path)
    {
        Json j;
        j["timestamp"] = tokens.timestamp;
        j["playerTokens"] = Json::parse(tokens.playerTokens, nullptr, false);

        std::ofstream out(path);
        if (!out) return false;
        out << j.dump(2);
        std::cout << "[+] Tokens saved to: " << path.string() << std::endl;
        return true;
    }

    std::optional<CapturedTokens> loadTokens(const fs::path& path)
    {
        if (!fs::exists(path)) return std::nullopt;

        std::ifstream in(path);
        if (!in) return std::nullopt;

        std::string content((std::istreambuf_iterator<char>(in)),
            std::istreambuf_iterator<char>());

        try
        {
            Json j = Json::parse(content);
            CapturedTokens tokens;
            tokens.timestamp = j.value("timestamp", "");
            tokens.playerTokens = j["playerTokens"].dump(2);
            return tokens;
        }
        catch (...)
        {
            return std::nullopt;
        }
    }
}

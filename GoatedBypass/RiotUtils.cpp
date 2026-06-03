#include "RiotUtils.hpp"

#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <cstdlib>
#include <windows.h>
#include <winsvc.h>
#include <tlhelp32.h>

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace 
{
    std::optional<std::string> GetEnv(const char* name) 
    {
        if (const char* v = std::getenv(name)) 
            return std::string(v);
        return std::nullopt;
    }

    fs::path GetProgramFilesDir()
    {
        if (auto p = GetEnv("ProgramW6432"))  
            return fs::path(*p);
        if (auto p = GetEnv("PROGRAMFILES"))  
            return fs::path(*p);

        return fs::path("C:\\Program Files");
    }

    bool IsServiceRunning(const wchar_t* serviceName)
    {
        SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
        if (!scm) 
            return false;
        SC_HANDLE svc = OpenServiceW(scm, serviceName, SERVICE_QUERY_STATUS);
        if (!svc)
        {
            CloseServiceHandle(scm);
            return false;
        }

        SERVICE_STATUS_PROCESS ssp{};
        DWORD bytesNeeded = 0;
        BOOL ok = QueryServiceStatusEx(svc,
            SC_STATUS_PROCESS_INFO,
            reinterpret_cast<LPBYTE>(&ssp),
            sizeof(ssp),
            &bytesNeeded);

        CloseServiceHandle(svc);
        CloseServiceHandle(scm);

        if (!ok)
            return false;

        return ssp.dwCurrentState == SERVICE_RUNNING ||
            ssp.dwCurrentState == SERVICE_START_PENDING;
    }
} 

std::optional<std::filesystem::path> RiotUtils::getPath()
{
    fs::path programData;

    if (auto env = GetEnv("PROGRAMDATA")) 
    {
        programData = *env;
    }
    else 
    {
        programData = "C:\\ProgramData";
    }

    const fs::path installJson = programData / "Riot Games" / "RiotClientInstalls.json";

    if (!fs::exists(installJson))
    {
        return std::nullopt;
    }

    try {
        std::ifstream in(installJson, std::ios::binary);
        if (!in)
        {
            std::cerr << "[WARN] Could not open " << installJson.string() << "\n";
            return std::nullopt;
        }

        json j;
        in >> j;

        std::vector<std::string> keys = { "rc_default", "rc_live", "rc_beta" };
        for (const auto& key : keys) 
        {
            if (j.contains(key) && j[key].is_string()) 
            {
                fs::path candidate = j[key].get<std::string>();
                if (!candidate.empty() && fs::exists(candidate)) 
                {
                    return candidate;
                }
            }
        }
    }

    catch (...) 
    {
    }

    return std::nullopt;
}

bool RiotUtils::removeVanguard()
{
    const fs::path vgkPath = GetProgramFilesDir() / "Riot Vanguard" / "installer.exe";

    if (!fs::exists(vgkPath)) 
        return true; 

    std::wstring exe = vgkPath.wstring();
    std::wstring cmd = L"\"" + exe + L"\" --quiet";

    STARTUPINFOW si{};
    PROCESS_INFORMATION pi{};
    si.cb = sizeof(si);

    std::vector<wchar_t> cmdBuf(cmd.begin(), cmd.end());
    cmdBuf.push_back(L'\0');

    BOOL ok = CreateProcessW(
        /*lpApplicationName*/ nullptr,
        /*lpCommandLine*/    cmdBuf.data(),
        /*lpProcessAttributes*/ nullptr,
        /*lpThreadAttributes*/  nullptr,
        /*bInheritHandles*/ FALSE,
        /*dwCreationFlags*/ 0,
        /*lpEnvironment*/   nullptr,
        /*lpCurrentDirectory*/ nullptr,
        /*lpStartupInfo*/   &si,
        /*lpProcessInformation*/ &pi
    );

    if (!ok) 
        return false;

    WaitForSingleObject(pi.hProcess, INFINITE);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    Sleep(5000);

    for (int i = 0; i < 30; ++i) 
    {
        if (!fs::exists(vgkPath))
            return true;

        Sleep(1000);
    }

    return false;
}

bool RiotUtils::isVanguardInstalled()
{
    const fs::path vgkPath = GetProgramFilesDir() / "Riot Vanguard" / "installer.exe";
    const bool pathExists = fs::exists(vgkPath);

    const bool vgcRunning = IsServiceRunning(L"vgc");
    const bool vgkRunning = IsServiceRunning(L"vgk");

    return pathExists || vgcRunning || vgkRunning;
}

bool RiotUtils::runRiotClient(uint16_t configPort)
{
	auto riotPath = getPath();
    if (!riotPath.has_value()) 
        return false;
    
    fs::path clientExe = *riotPath;
    if (!fs::exists(clientExe)) 
        return false;

    std::wstring cmd = L"\"" + clientExe.wstring() + L"\" --client-config-url=http://127.0.0.1:" + std::to_wstring(configPort);

    STARTUPINFOW si{};
    PROCESS_INFORMATION pi{};
    si.cb = sizeof(si);
    std::vector<wchar_t> cmdBuf(cmd.begin(), cmd.end());
    cmdBuf.push_back(L'\0');
    BOOL ok = CreateProcessW(
        /*lpApplicationName*/ nullptr,
        /*lpCommandLine*/    cmdBuf.data(),
        /*lpProcessAttributes*/ nullptr,
        /*lpThreadAttributes*/  nullptr,
        /*bInheritHandles*/ FALSE,
        /*dwCreationFlags*/ 0,
        /*lpEnvironment*/   nullptr,
        /*lpCurrentDirectory*/ nullptr,
        /*lpStartupInfo*/   &si,
        /*lpProcessInformation*/ &pi
    );

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

	return true;
}

void RiotUtils::patchProductSettings()
{
    auto env = GetEnv("PROGRAMDATA");
    fs::path programData = env ? fs::path(*env) : fs::path("C:\\ProgramData");

    const fs::path metaDir = programData / "Riot Games" / "Metadata";
    if (!fs::exists(metaDir)) return;

    for (const auto& entry : fs::recursive_directory_iterator(metaDir))
    {
        if (entry.path().extension() != ".yaml") continue;
        if (entry.path().filename().string().find("product_settings") == std::string::npos) continue;

        std::ifstream in(entry.path());
        if (!in) continue;

        std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        in.close();

        bool patched = false;
        auto pos = content.find("vanguard: true");
        while (pos != std::string::npos)
        {
            content.replace(pos, std::string("vanguard: true").size(), "vanguard: false");
            patched = true;
            pos = content.find("vanguard: true", pos + 1);
        }

        if (patched)
        {
            std::ofstream out(entry.path());
            out << content;
            std::cout << "[+] Patched: " << entry.path().filename().string() << std::endl;
        }
    }
}

void RiotUtils::terminateRiotServices()
{
	std::vector<std::wstring> names = { L"RiotClientServices.exe", L"RiotClient.exe", L"LeagueClient.exe", L"League of Legends.exe", L"VALORANT.exe", L"RiotClientUx.exe", L"RiotClientCrashHandler.exe" };

	// Terminate all Riot services
    for (const auto& name : names) 
    {
        HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (hSnap == INVALID_HANDLE_VALUE) continue;
        PROCESSENTRY32 pe{};
        pe.dwSize = sizeof(pe);
        if (Process32First(hSnap, &pe)) 
        {
            do 
            {
                if (std::wstring(pe.szExeFile) == name) 
                {
                    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pe.th32ProcessID);
                    if (hProcess) 
                    {
                        TerminateProcess(hProcess, 0);
                        CloseHandle(hProcess);
                    }
                }
			} while (Process32Next(hSnap, &pe));
        }
        CloseHandle(hSnap);
	}
}

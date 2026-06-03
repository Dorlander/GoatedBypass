#include "FakeVanguard.hpp"
#include <winsvc.h>
#include <iostream>

namespace
{
    SERVICE_STATUS_HANDLE g_vgcHandle = nullptr;
    SERVICE_STATUS_HANDLE g_vgkHandle = nullptr;

    void setRunning(SERVICE_STATUS_HANDLE h)
    {
        SERVICE_STATUS ss{};
        ss.dwServiceType      = SERVICE_WIN32_OWN_PROCESS;
        ss.dwCurrentState     = SERVICE_RUNNING;
        ss.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
        ss.dwWin32ExitCode    = NO_ERROR;
        SetServiceStatus(h, &ss);
    }

    void setStopped(SERVICE_STATUS_HANDLE h)
    {
        SERVICE_STATUS ss{};
        ss.dwServiceType  = SERVICE_WIN32_OWN_PROCESS;
        ss.dwCurrentState = SERVICE_STOPPED;
        ss.dwWin32ExitCode = NO_ERROR;
        SetServiceStatus(h, &ss);
    }

    DWORD WINAPI serviceCtrl(DWORD ctrl, DWORD, LPVOID, LPVOID ctx)
    {
        auto h = reinterpret_cast<SERVICE_STATUS_HANDLE>(ctx);
        if (ctrl == SERVICE_CONTROL_STOP || ctrl == SERVICE_CONTROL_SHUTDOWN)
            setStopped(h);
        return NO_ERROR;
    }

    void WINAPI vgcMain(DWORD, LPWSTR*)
    {
        g_vgcHandle = RegisterServiceCtrlHandlerExW(L"vgc",
            serviceCtrl, reinterpret_cast<LPVOID>(g_vgcHandle));
        if (!g_vgcHandle) return;
        setRunning(g_vgcHandle);
        Sleep(INFINITE);
    }

    void WINAPI vgkMain(DWORD, LPWSTR*)
    {
        g_vgkHandle = RegisterServiceCtrlHandlerExW(L"vgk",
            serviceCtrl, reinterpret_cast<LPVOID>(g_vgkHandle));
        if (!g_vgkHandle) return;
        setRunning(g_vgkHandle);
        Sleep(INFINITE);
    }

    bool createService(SC_HANDLE scm, const wchar_t* name, const wchar_t* displayName)
    {
        wchar_t exePath[MAX_PATH];
        GetModuleFileNameW(nullptr, exePath, MAX_PATH);

        std::wstring binPath = std::wstring(L"\"") + exePath + L"\" --service=" + name;

        SC_HANDLE svc = CreateServiceW(
            scm, name, displayName,
            SERVICE_ALL_ACCESS,
            SERVICE_WIN32_OWN_PROCESS,
            SERVICE_DEMAND_START,
            SERVICE_ERROR_NORMAL,
            binPath.c_str(),
            nullptr, nullptr, nullptr, nullptr, nullptr
        );

        if (!svc)
        {
            DWORD err = GetLastError();
            if (err == ERROR_SERVICE_EXISTS)
                return true;
            std::cerr << "[!] Failed to create service " << (char*)name << ": " << err << "\n";
            return false;
        }

        StartServiceW(svc, 0, nullptr);
        CloseServiceHandle(svc);
        return true;
    }

    void deleteService(SC_HANDLE scm, const wchar_t* name)
    {
        SC_HANDLE svc = OpenServiceW(scm, name, SERVICE_ALL_ACCESS);
        if (!svc) return;

        SERVICE_STATUS ss{};
        ControlService(svc, SERVICE_CONTROL_STOP, &ss);
        DeleteService(svc);
        CloseServiceHandle(svc);
    }
}

namespace FakeVanguard
{
    bool install()
    {
        SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_ALL_ACCESS);
        if (!scm)
        {
            std::cerr << "[!] OpenSCManager failed (need admin): " << GetLastError() << "\n";
            return false;
        }

        bool ok = createService(scm, L"vgc", L"Vanguard Service") &&
                  createService(scm, L"vgk", L"Vanguard Kernel Service");

        CloseServiceHandle(scm);
        return ok;
    }

    void uninstall()
    {
        SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_ALL_ACCESS);
        if (!scm) return;

        deleteService(scm, L"vgc");
        deleteService(scm, L"vgk");
        CloseServiceHandle(scm);
    }
}

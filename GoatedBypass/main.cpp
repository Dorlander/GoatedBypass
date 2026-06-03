#include "ConfigProxy.hpp"
#include "RiotUtils.hpp"
#include "TokenCapture.hpp"

#include <iostream>
#include <thread>
#include <csignal>
#include <string>

BOOL WINAPI consoleCtrlHandler(DWORD)
{
    RiotUtils::terminateRiotServices();
    return TRUE;
}

void signalHandler(int)
{
    RiotUtils::terminateRiotServices();
}

std::terminate_handler g_prevTerminate = nullptr;
void terminateHandler()
{
    RiotUtils::terminateRiotServices();
    if (g_prevTerminate)
        g_prevTerminate();
    std::abort();
}

void printUsage()
{
    std::cout << "GoatedBypass v2.0\n"
              << "Usage:\n"
              << "  GoatedBypass.exe           - Normal bypass mode\n"
              << "  GoatedBypass.exe capture   - Capture tokens from running client (Vanguard must be active)\n"
              << "  GoatedBypass.exe replay    - Bypass + replay saved tokens\n"
              << std::endl;
}

int modeCapture()
{
    std::cout << "[*] Token Capture Mode" << std::endl;
    std::cout << "[*] Make sure Riot Client is running WITH Vanguard!" << std::endl;
    std::cout << "[*] Press Enter when ready..." << std::endl;
    std::cin.get();

    auto tokens = TokenCapture::capture();
    if (!tokens)
    {
        std::cerr << "[!] Failed to capture tokens!" << std::endl;
        return 1;
    }

    TokenCapture::saveTokens(*tokens, "captured_tokens.json");

    std::cout << "\n[+] Done! Now you can run: GoatedBypass.exe replay" << std::endl;
    return 0;
}

int modeReplay()
{
    std::cout << "[*] Replay Mode - loading saved tokens..." << std::endl;

    auto tokens = TokenCapture::loadTokens("captured_tokens.json");
    if (!tokens)
    {
        std::cerr << "[!] No saved tokens found! Run 'GoatedBypass.exe capture' first." << std::endl;
        return 1;
    }

    std::cout << "[+] Tokens loaded (captured at: " << tokens->timestamp << ")" << std::endl;

    // Normal bypass flow
    std::atexit([] { RiotUtils::terminateRiotServices(); });
    g_prevTerminate = std::set_terminate(terminateHandler);
    SetConsoleCtrlHandler(consoleCtrlHandler, TRUE);
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);
    std::signal(SIGABRT, signalHandler);

    RiotUtils::terminateRiotServices();

    auto riotPath = RiotUtils::getPath();
    if (!riotPath.has_value())
    {
        std::cout << "Can't find Riot Client!" << std::endl;
        return 1;
    }

    if (RiotUtils::isVanguardInstalled())
    {
        if (!RiotUtils::removeVanguard())
        {
            std::cout << "Failed to remove Vanguard!" << std::endl;
            return 1;
        }
    }

    if (RiotUtils::isVanguardInstalled())
    {
        std::cout << "Vanguard is still installed!" << std::endl;
        return 1;
    }

    RiotUtils::patchProductSettings();

    // TODO: Pass captured tokens to ConfigProxy for injection
    ConfigProxy proxy;
    std::thread t([&] { proxy.run(); });

    std::this_thread::sleep_for(std::chrono::seconds(3));
    RiotUtils::runRiotClient(proxy.getPort());

    std::cout << "[*] Bypass running with captured tokens" << std::endl;

    while (true)
        std::this_thread::sleep_for(std::chrono::seconds(5));

    proxy.stop();
    t.join();
    return 0;
}

int modeNormal()
{
    std::atexit([] { RiotUtils::terminateRiotServices(); });
    g_prevTerminate = std::set_terminate(terminateHandler);
    SetConsoleCtrlHandler(consoleCtrlHandler, TRUE);
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);
    std::signal(SIGABRT, signalHandler);

    RiotUtils::terminateRiotServices();

    auto riotPath = RiotUtils::getPath();
    if (!riotPath.has_value())
    {
        std::cout << "Can't find Riot Client!" << std::endl;
        return 1;
    }

    if (RiotUtils::isVanguardInstalled())
    {
        if (!RiotUtils::removeVanguard())
        {
            std::cout << "Failed to remove Vanguard!" << std::endl;
            return 1;
        }
    }

    if (RiotUtils::isVanguardInstalled())
    {
        std::cout << "Vanguard is still installed!" << std::endl;
        return 1;
    }

    RiotUtils::patchProductSettings();

    ConfigProxy proxy;
    std::thread t([&] { proxy.run(); });

    std::this_thread::sleep_for(std::chrono::seconds(3));
    RiotUtils::runRiotClient(proxy.getPort());

    std::cout << "Goodbye cheap bypass!" << std::endl;

    while (true)
        std::this_thread::sleep_for(std::chrono::seconds(5));

    proxy.stop();
    t.join();
    return 0;
}

int main(int argc, char* argv[])
{
    printUsage();

    if (argc > 1)
    {
        std::string mode = argv[1];
        if (mode == "capture") return modeCapture();
        if (mode == "replay")  return modeReplay();
    }

    return modeNormal();
}

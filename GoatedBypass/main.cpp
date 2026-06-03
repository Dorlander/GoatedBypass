#include "ConfigProxy.hpp"
#include "RiotUtils.hpp"
#include "FakeVanguard.hpp"

#include <iostream>
#include <thread>
#include <csignal>

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

int main() 
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

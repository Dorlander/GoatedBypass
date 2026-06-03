#pragma once

#include <filesystem>
#include <optional>
#include <string>

namespace RiotUtils
{
	extern std::optional<std::filesystem::path> getPath();
	extern bool removeVanguard();
	extern bool isVanguardInstalled();
	extern bool runRiotClient(uint16_t configPort);
	extern void terminateRiotServices();
	extern void patchProductSettings();
}

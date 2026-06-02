#pragma once
#include <nlohmann/json.hpp>
#include <string>

class JsonPatcher 
{
public:
    static std::string patchNovgkOnly(const std::string& body);

private:
    using Json = nlohmann::json;

    static void setKey(Json& cfg, const std::string& key, const Json& value);
    static void removeVanguardDependencies(Json& cfg, const std::string& path);
};

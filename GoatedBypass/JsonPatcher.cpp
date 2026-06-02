#include "JsonPatcher.hpp"


using Json = nlohmann::json;

void JsonPatcher::setKey(Json& cfg, const std::string& key, const Json& value)
{
    if (cfg.contains(key)) cfg[key] = value;
}

void JsonPatcher::removeVanguardDependencies(Json& cfg, const std::string& path)
{
    if (!cfg.contains(path)) 
        return;

    auto& product = cfg[path];
    if (!product.contains("platforms"))
        return;
    if (!product["platforms"].contains("win")) 
        return;
    if (!product["platforms"]["win"].contains("configurations")) 
        return;

    auto& configs = product["platforms"]["win"]["configurations"];
    if (!configs.is_array())
        return;

    for (auto& conf : configs)
    {
        if (conf.contains("dependencies") && conf["dependencies"].is_array())
        {
            auto& deps = conf["dependencies"];
            for (auto it = deps.begin(); it != deps.end();)
            {
                if (it->contains("id") && (*it)["id"].is_string() &&
                    (*it)["id"].get<std::string>() == "vanguard") 
                {
                    it = deps.erase(it);
                }
                else
                {
                    ++it;
                }
            }
        }
    }
}

std::string JsonPatcher::patchNovgkOnly(const std::string& body) 
{
    Json j = Json::parse(body, nullptr, false);
    if (j.is_discarded())
        return body;

    setKey(j, "anticheat.vanguard.backgroundInstall", false);
    setKey(j, "anticheat.vanguard.enabled", false);
    setKey(j, "keystone.client.feature_flags.restart_required.disabled", true);
    setKey(j, "keystone.client.feature_flags.vanguardLaunch.disabled", true);
    setKey(j, "keystone.client.feature_flags.vanguard_attestation.enabled", false);
    setKey(j, "lol.client_settings.vanguard.enabled", false);
    setKey(j, "lol.client_settings.vanguard.enabled_embedded", false);
    setKey(j, "lol.client_settings.vanguard.url", "");
    setKey(j, "lion.vanguard.required", false);
    setKey(j, "lion.vanguard.netrequired", false);

    removeVanguardDependencies(j, "keystone.products.lion.patchlines.live");
    removeVanguardDependencies(j, "keystone.products.league_of_legends.patchlines.live");
    removeVanguardDependencies(j, "keystone.products.league_of_legends.patchlines.pbe");
    removeVanguardDependencies(j, "keystone.products.valorant.patchlines.live");

    return j.dump();
}


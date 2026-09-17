#include "musicbox/config/Config.hpp"

#include <toml++/toml.h>

#include <stdexcept>

namespace musicbox::config {

Config loadConfigFile(const std::string& path) {
    // With exceptions enabled (the toml++ default, and this build's), parse_result
    // is aliased directly to toml::table and toml::parse_file() throws
    // toml::parse_error on failure rather than returning a discriminated result --
    // re-thrown here as std::runtime_error to match this header's documented
    // contract and keep toml++ an implementation detail of this one file.
    toml::table root;
    try {
        root = toml::parse_file(path);
    } catch (const toml::parse_error& e) {
        throw std::runtime_error("failed to parse config file '" + path +
                                 "': " + std::string(e.description()));
    }

    Config config; // start from built-in defaults; the file only overrides what it sets

    if (auto server = root["server"].as_table()) {
        config.server.address = (*server)["address"].value_or(config.server.address);
        config.server.port = static_cast<std::uint16_t>(
            (*server)["port"].value_or<std::int64_t>(config.server.port));
    }

    if (auto library = root["library"].as_table()) {
        if (auto paths = (*library)["paths"].as_array()) {
            config.library.paths.clear();
            for (const auto& element : *paths) {
                if (auto path_str = element.value<std::string>()) {
                    config.library.paths.push_back(*path_str);
                }
            }
        }
    }

    if (auto database = root["database"].as_table()) {
        config.database.path = (*database)["path"].value_or(config.database.path);
    }

    if (auto network = root["network"].as_table()) {
        config.network.mdnsName = (*network)["mdns_name"].value_or(config.network.mdnsName);
    }

    if (auto logging = root["logging"].as_table()) {
        config.logging.level = (*logging)["level"].value_or(config.logging.level);
    }

    return config;
}

} // namespace musicbox::config

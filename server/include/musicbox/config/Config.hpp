#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace musicbox::config {

// Mirrors the TOML schema in Plan.md §19 / docs/development.md. Precedence:
// built-in defaults < config file < CLI flag overrides.
struct Config {
    struct Server {
        std::string address = "0.0.0.0";
        std::uint16_t port = 8080;
    } server;

    struct Library {
        std::vector<std::string> paths;
    } library;

    struct Database {
        std::string path = "musicbox.db";
    } database;

    struct Network {
        std::string mdnsName = "musicbox";
    } network;

    struct Logging {
        std::string level = "info";
    } logging;
};

// Loads a TOML file at `path`, applying it on top of Config{} defaults. Throws
// std::runtime_error on a missing/unparsable file.
[[nodiscard]] Config loadConfigFile(const std::string& path);

} // namespace musicbox::config

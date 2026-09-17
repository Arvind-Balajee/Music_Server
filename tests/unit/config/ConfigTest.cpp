#include "musicbox/config/Config.hpp"

#include <catch2/catch_test_macros.hpp>
#include <cstdio>
#include <fstream>

using namespace musicbox::config;

namespace {

class TempConfigFile {
public:
    explicit TempConfigFile(const std::string& contents) {
        std::ofstream out(path_);
        out << contents;
    }
    ~TempConfigFile() { std::remove(path_.c_str()); }

    [[nodiscard]] const std::string& path() const { return path_; }

private:
    std::string path_ = "musicbox_config_test.toml";
};

} // namespace

TEST_CASE("loadConfigFile throws for a missing file", "[config]") {
    CHECK_THROWS_AS(loadConfigFile("/nonexistent/path/musicbox.toml"), std::runtime_error);
}

TEST_CASE("loadConfigFile throws for unparsable TOML", "[config]") {
    TempConfigFile file("this is not [ valid toml");
    CHECK_THROWS_AS(loadConfigFile(file.path()), std::runtime_error);
}

TEST_CASE("loadConfigFile overrides only the fields present in the file", "[config]") {
    TempConfigFile file(R"toml(
[server]
port = 9090

[library]
paths = ["/media/music", "/mnt/nas/music"]
)toml");

    const Config config = loadConfigFile(file.path());

    CHECK(config.server.port == 9090);
    CHECK(config.server.address == "0.0.0.0"); // untouched -- default preserved
    REQUIRE(config.library.paths.size() == 2);
    CHECK(config.library.paths[0] == "/media/music");
    CHECK(config.library.paths[1] == "/mnt/nas/music");
    CHECK(config.database.path == "musicbox.db"); // untouched -- default preserved
}

TEST_CASE("loadConfigFile reads every documented field", "[config]") {
    TempConfigFile file(R"toml(
[server]
address = "127.0.0.1"
port = 8081

[library]
paths = ["/music"]

[database]
path = "/var/lib/musicbox/musicbox.db"

[network]
mdns_name = "mybox"

[logging]
level = "debug"
)toml");

    const Config config = loadConfigFile(file.path());

    CHECK(config.server.address == "127.0.0.1");
    CHECK(config.server.port == 8081);
    REQUIRE(config.library.paths.size() == 1);
    CHECK(config.library.paths[0] == "/music");
    CHECK(config.database.path == "/var/lib/musicbox/musicbox.db");
    CHECK(config.network.mdnsName == "mybox");
    CHECK(config.logging.level == "debug");
}

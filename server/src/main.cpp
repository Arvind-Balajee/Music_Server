#include <cstdint>
#include <cstring>
#include <iostream>
#include <span>
#include <string>
#include <string_view>

#include "musicbox/net/EventLoop.hpp"
#include "musicbox/net/TcpConnection.hpp"

namespace {

constexpr std::string_view kVersion = "0.1.0";

void printUsage() {
    std::cout << "Usage: musicbox-server <command> [--config <path>]\n"
                 "Commands:\n"
                 "  run       Start the server (event loop + HTTP API)\n"
                 "  scan      Run the library scanner once and exit\n"
                 "  status    Query a running instance's /api/v1/status\n"
                 "  doctor    Validate config, DB, and library root permissions\n"
                 "  version   Print the server version\n";
}

// ---------------------------------------------------------------------------
// TEMPORARY Milestone-1 demo wiring (Agent 1 / networking core).
//
// This is a placeholder so `musicbox-server run` does something end-to-end
// verifiable (curl returns "MusicBox") before Agent 2's real HttpRequest
// parser + Router land on top of musicbox::net. It has zero HTTP semantics:
// as soon as *any* bytes arrive on a connection, it writes back one fixed,
// valid HTTP/1.1 response and closes. Agent 2 (and/or the integration lead)
// should delete this block and instead wire EventLoop::onReadable to feed
// bytes into an HttpRequestParser + Router per docs/http.md.
//
// The listen port is hardcoded rather than read from
// musicbox::config::Config::Server::port because Config.cpp/TOML loading
// has not been wired into main() yet (see "Integration dependencies" in
// Agent 1's final report) — 8080 matches Config::Server's own default.
// ---------------------------------------------------------------------------
void runTemporaryDemoServer(std::uint16_t port) {
    using musicbox::net::EventLoop;
    using musicbox::net::TcpConnection;

    constexpr std::string_view kResponse =
        "HTTP/1.1 200 OK\r\nContent-Length: 8\r\nConnection: close\r\n\r\nMusicBox";

    EventLoop loop;
    loop.onReadable([kResponse](TcpConnection& connection) {
        // Don't parse the request at all -- just require that *something*
        // arrived, then respond and close. The real HTTP layer replaces this
        // entirely.
        auto& inbound = connection.inboundBuffer();
        inbound.consume(inbound.readableBytes());

        auto bytes = std::as_bytes(std::span<const char>(kResponse.data(), kResponse.size()));
        connection.queueWrite(bytes);
        connection.closeAfterFlush();
    });

    std::cout << "musicbox-server: listening on 0.0.0.0:" << port
              << " (TEMPORARY Milestone-1 demo handler -- see server/src/main.cpp)" << std::endl;
    loop.listen("0.0.0.0", port);
    loop.run();
}

int runCommand(std::string_view command) {
    if (command == "version") {
        std::cout << "musicbox-server " << kVersion << '\n';
        return 0;
    }
    if (command == "run") {
        runTemporaryDemoServer(8080);
        return 0;
    }
    if (command == "scan" || command == "status" || command == "doctor") {
        // Placeholder until Agent 3 (library/db) and Agent 4 (API) land their
        // pieces — see docs/architecture.md for the composition root this
        // will wire together (EventLoop + Router + SqliteRepositories).
        std::cerr << "musicbox-server: '" << command
                  << "' is not implemented yet (see Plan.md milestones 1-6)\n";
        return 1;
    }
    std::cerr << "musicbox-server: unknown command '" << command << "'\n";
    printUsage();
    return 2;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        printUsage();
        return 2;
    }
    return runCommand(argv[1]);
}

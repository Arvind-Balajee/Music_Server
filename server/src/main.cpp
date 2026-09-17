#include <cstdint>
#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>

#include "musicbox/api/Api.hpp"
#include "musicbox/config/Config.hpp"
#include "musicbox/db/AlbumRepository.hpp"
#include "musicbox/db/ArtistRepository.hpp"
#include "musicbox/db/LibraryRootRepository.hpp"
#include "musicbox/db/PlaylistRepository.hpp"
#include "musicbox/db/TrackRepository.hpp"
#include "musicbox/http/HttpRequestParser.hpp"
#include "musicbox/http/HttpResponse.hpp"
#include "musicbox/http/HttpStatus.hpp"
#include "musicbox/http/Router.hpp"
#include "musicbox/library/LibraryScanner.hpp"
#include "musicbox/library/MetadataExtractor.hpp"
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
                 "  version   Print the server version\n"
                 "\n"
                 "Options:\n"
                 "  --config <path>   Load a TOML config file (default: built-in defaults --\n"
                 "                    0.0.0.0:8080, no library paths, ./musicbox.db).\n";
}

struct CliArgs {
    std::string command;
    std::optional<std::string> configPath;
};

std::optional<CliArgs> parseArgs(int argc, char** argv) {
    if (argc < 2) {
        return std::nullopt;
    }
    CliArgs args;
    args.command = argv[1];
    for (int i = 2; i < argc; ++i) {
        std::string_view arg = argv[i];
        if (arg == "--config" && i + 1 < argc) {
            args.configPath = argv[++i];
        }
    }
    return args;
}

// ---------------------------------------------------------------------------
// HTTP response serialization
// ---------------------------------------------------------------------------

// Serializes `response` onto `connection` and queues it for sending. Adds
// Connection: close unconditionally (docs/adr/0009-http-connection-close-only.md
// -- this composition doesn't support persistent connections yet) and
// Content-Length for non-streaming bodies (streaming responses set their own
// Content-Length in server/src/api/Api.cpp, derived from the resolved byte
// range, since this function has no way to know it otherwise).
//
// `suppressBody` implements the wire-level half of HEAD support (the other
// half is Router::dispatch() matching HEAD against the GET route -- see its
// comment in server/src/http/Router.cpp): `response` here is exactly what the
// equivalent GET would have produced, so the headers below (including
// Content-Length/Content-Range) are correct as-is; RFC 7231 §4.3.2 just
// forbids actually sending the body bytes that would follow them.
void writeHttpResponse(musicbox::net::TcpConnection& connection,
                       musicbox::http::HttpResponse response, bool suppressBody = false) {
    using musicbox::http::reasonPhrase;

    if (!response.isStreaming()) {
        response.headers["Content-Length"] = std::to_string(response.body.size());
    }
    response.headers["Connection"] = "close";

    std::string head = "HTTP/1.1 " + std::to_string(static_cast<int>(response.statusCode)) + " " +
                       reasonPhrase(response.statusCode) + "\r\n";
    for (const auto& [key, value] : response.headers) {
        head += key + ": " + value + "\r\n";
    }
    head += "\r\n";
    connection.queueWrite(std::as_bytes(std::span<const char>(head.data(), head.size())));

    if (suppressBody) {
        // Headers are already queued above; dropping `response` here (its
        // streamingBody, if any) releases the underlying file handle without
        // ever pulling a byte from it.
        connection.closeAfterFlush();
        return;
    }

    if (response.isStreaming()) {
        // Adapted into TcpConnection's pull-based streaming body
        // (docs/adr/0008-tcpconnection-streaming-body.md) so a large audio
        // file is sent in bounded chunks rather than buffered whole. A
        // shared_ptr is required here (not the unique_ptr HttpResponse itself
        // holds) because std::function needs a copyable target.
        std::shared_ptr<musicbox::http::ResponseBody> body(std::move(response.streamingBody));
        connection.setStreamingBody(
            [body](std::span<std::byte> dst) -> std::size_t { return body->read(dst); });
    } else if (!response.body.empty()) {
        connection.queueWrite(
            std::span<const std::byte>(response.body.data(), response.body.size()));
    }

    connection.closeAfterFlush();
}

// ---------------------------------------------------------------------------
// `run`
// ---------------------------------------------------------------------------

void runServer(const musicbox::config::Config& config) {
    using namespace musicbox;

    auto trackRepo = db::makeSqliteTrackRepository(config.database.path);
    auto artistRepo = db::makeSqliteArtistRepository(config.database.path);
    auto albumRepo = db::makeSqliteAlbumRepository(config.database.path);
    auto playlistRepo = db::makeSqlitePlaylistRepository(config.database.path);
    auto libraryRootRepo = db::makeSqliteLibraryRootRepository(config.database.path);

    auto router = http::createRouter();
    api::registerApiRoutes(*router, *trackRepo, *artistRepo, *albumRepo, *playlistRepo,
                           *libraryRootRepo);

    net::EventLoop loop;
    // One HttpRequestParser per live connection. Erased on close (below) so
    // this doesn't grow unbounded across the server's lifetime; safe to key
    // by raw TcpConnection* since EventLoop owns each connection by
    // unique_ptr in a stable-address map for its whole lifetime (see
    // EventLoop.cpp's `connections_`).
    std::unordered_map<const net::TcpConnection*, http::HttpRequestParser> parsers;

    loop.onReadable([&](net::TcpConnection& connection) {
        auto& inbound = connection.inboundBuffer();
        auto view = inbound.readableView();

        auto& parser = parsers[&connection]; // default-constructs on first use
        parser.feed(view);
        inbound.consume(view.size());

        auto result = parser.next();
        if (result.status == http::ParseStatus::NeedMoreData) {
            return; // wait for more bytes on the next readable event
        }

        http::HttpResponse response;
        bool isHeadRequest = false;
        if (result.status == http::ParseStatus::Error) {
            response = http::HttpResponse::error(result.errorStatus, "BAD_REQUEST",
                                                 "Malformed HTTP request.");
            std::cout << "-> " << static_cast<int>(result.errorStatus) << " (parse error)"
                      << std::endl;
        } else {
            isHeadRequest = result.request.method == "HEAD";
            response = router->dispatch(result.request);
            std::cout << result.request.method << ' ' << result.request.path << " -> "
                      << static_cast<int>(response.statusCode) << std::endl;
        }

        // One response per connection (docs/adr/0009) -- any further buffered
        // bytes (a pipelined next request) are simply never read; the
        // connection closes once this response drains.
        writeHttpResponse(connection, std::move(response), isHeadRequest);
    });

    loop.onClosed([&](net::TcpConnection& connection) { parsers.erase(&connection); });

    std::cout << "musicbox-server: listening on " << config.server.address << ':'
              << config.server.port << " (" << trackRepo->count(db::TrackQuery{})
              << " tracks indexed)" << std::endl;
    loop.listen(config.server.address, config.server.port);
    loop.run();
}

// ---------------------------------------------------------------------------
// `scan`
// ---------------------------------------------------------------------------

int runScan(const musicbox::config::Config& config) {
    using namespace musicbox;

    if (config.library.paths.empty()) {
        std::cerr << "musicbox-server: no [library].paths configured -- nothing to scan. "
                     "Pass --config with a musicbox.toml that lists at least one path.\n";
        return 1;
    }

    auto trackRepo = db::makeSqliteTrackRepository(config.database.path);
    auto libraryRootRepo = db::makeSqliteLibraryRootRepository(config.database.path);
    auto metadataExtractor = library::makeDefaultMetadataExtractor();
    auto scanner = library::makeLibraryScanner(*trackRepo, *metadataExtractor);

    for (const auto& path : config.library.paths) {
        std::error_code ec;
        if (!std::filesystem::exists(path, ec) || ec) {
            std::cerr << "musicbox-server: library path '" << path
                      << "' does not exist, skipping\n";
            continue;
        }

        auto root = libraryRootRepo->upsert(path);
        std::cout << "Scanning " << path << " ...\n";
        auto result = scanner->scan(root);
        libraryRootRepo->markScanned(root.id, std::chrono::system_clock::now());

        std::cout << "  inserted=" << result.inserted << " updated=" << result.updated
                  << " unchanged=" << result.unchanged << " deleted=" << result.deleted
                  << " skipped=" << result.skippedUnreadable << '\n';
    }
    return 0;
}

// ---------------------------------------------------------------------------

int runCommand(const CliArgs& args) {
    if (args.command == "version") {
        std::cout << "musicbox-server " << kVersion << '\n';
        return 0;
    }

    musicbox::config::Config config; // built-in defaults
    if (args.configPath) {
        try {
            config = musicbox::config::loadConfigFile(*args.configPath);
        } catch (const std::exception& e) {
            std::cerr << "musicbox-server: " << e.what() << '\n';
            return 1;
        }
    }

    if (args.command == "run") {
        runServer(config);
        return 0;
    }
    if (args.command == "scan") {
        return runScan(config);
    }
    if (args.command == "status" || args.command == "doctor") {
        // `status` needs an HTTP client and `doctor` needs a fuller set of
        // preflight checks -- neither is implemented yet. `run` + `scan` (and
        // curl/the iOS app against a running `run`) cover local testing today.
        std::cerr << "musicbox-server: '" << args.command << "' is not implemented yet.\n";
        return 1;
    }

    std::cerr << "musicbox-server: unknown command '" << args.command << "'\n";
    printUsage();
    return 2;
}

} // namespace

int main(int argc, char** argv) {
    auto args = parseArgs(argc, argv);
    if (!args) {
        printUsage();
        return 2;
    }
    return runCommand(*args);
}

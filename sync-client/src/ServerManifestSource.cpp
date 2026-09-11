#include "musicbox/sync/ServerManifestSource.hpp"

#include <chrono>
#include <stdexcept>

#include <nlohmann/json.hpp>

#include "musicbox/sync/HttpClient.hpp"

namespace musicbox::sync {

HttpServerManifestSource::HttpServerManifestSource(std::string serverHost, int defaultPort)
    : serverHost_(std::move(serverHost)), defaultPort_(defaultPort) {}

std::vector<ServerManifestEntry> HttpServerManifestSource::fetchManifest() {
    const SimpleHttpClient client(serverHost_, defaultPort_);

    // Proposed endpoint (docs/api.md, NOT YET IMPLEMENTED server-side):
    //   GET /api/v1/sync/manifest
    //   -> [ { "relativePath": "...", "size": N, "mtime": <unix seconds>,
    //          "contentHash": "<sha256 hex>" }, ... ]
    const HttpClientResponse response = client.get("/api/v1/sync/manifest");

    if (!response.connected) {
        throw std::runtime_error("HttpServerManifestSource: could not reach " + serverHost_ + ": " +
                                 response.error);
    }
    if (response.statusCode != 200) {
        throw std::runtime_error(
            "HttpServerManifestSource: server returned HTTP " +
            std::to_string(response.statusCode) +
            " for GET /api/v1/sync/manifest (endpoint may not be implemented yet)");
    }

    nlohmann::json parsed;
    try {
        parsed = nlohmann::json::parse(response.body);
    } catch (const nlohmann::json::parse_error& e) {
        throw std::runtime_error(
            std::string("HttpServerManifestSource: malformed JSON response: ") + e.what());
    }
    if (!parsed.is_array()) {
        throw std::runtime_error("HttpServerManifestSource: expected a JSON array response");
    }

    std::vector<ServerManifestEntry> entries;
    entries.reserve(parsed.size());
    for (const auto& item : parsed) {
        ServerManifestEntry entry;
        entry.relativePath = item.at("relativePath").get<std::string>();
        entry.sizeBytes = item.at("size").get<std::uint64_t>();
        const auto epochSeconds = item.at("mtime").get<std::int64_t>();
        entry.mtime = std::chrono::system_clock::time_point(std::chrono::seconds(epochSeconds));
        entry.contentHash = item.at("contentHash").get<std::string>();
        entries.push_back(std::move(entry));
    }
    return entries;
}

} // namespace musicbox::sync

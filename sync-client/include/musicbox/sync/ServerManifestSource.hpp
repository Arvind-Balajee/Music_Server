#pragma once

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

namespace musicbox::sync {

// One entry of the server's current track manifest. Mirrors the proposed
// GET /api/v1/sync/manifest response shape documented in docs/api.md
// ("Proposed: Sync Endpoints" section) — relativePath/size/mtime/contentHash.
// mtime is truncated to whole seconds (matches the JSON wire format: unix epoch
// seconds), which is enough precision for the size+mtime pre-check.
struct ServerManifestEntry {
    std::string relativePath;
    std::uint64_t sizeBytes = 0;
    std::chrono::system_clock::time_point mtime{};
    std::string contentHash;
};

// Source of the server's current track manifest, used by ManifestDiffer to
// compute what needs to change locally-vs-server. Kept as an interface
// (Plan.md §25) so ManifestDiffer/SyncEngine can be developed and tested without
// depending on a real server round trip.
class ServerManifestSource {
public:
    virtual ~ServerManifestSource() = default;
    [[nodiscard]] virtual std::vector<ServerManifestEntry> fetchManifest() = 0;
};

// Fixed in-memory manifest. Used by tests (and anywhere a known server state
// needs to be simulated) so diff logic can be exercised without a network call
// to an endpoint that doesn't exist on the server yet.
class MockServerManifestSource final : public ServerManifestSource {
public:
    explicit MockServerManifestSource(std::vector<ServerManifestEntry> entries)
        : entries_(std::move(entries)) {}

    [[nodiscard]] std::vector<ServerManifestEntry> fetchManifest() override { return entries_; }

private:
    std::vector<ServerManifestEntry> entries_;
};

// Talks to the *proposed* GET /api/v1/sync/manifest endpoint (docs/api.md;
// NOT YET IMPLEMENTED server-side — see the sync-client agent report). Throws
// std::runtime_error on a connection failure, a non-200 response, or malformed
// JSON. This exists so the wiring is ready the moment Agent 4 ships the
// endpoint; it is intentionally not exercised by the test suite beyond
// construction, since there's nothing real to talk to yet.
class HttpServerManifestSource final : public ServerManifestSource {
public:
    // serverHost may optionally include ":port" (e.g. "musicbox.local:9000");
    // otherwise defaultPort is used (8080, matching config/musicbox.toml).
    explicit HttpServerManifestSource(std::string serverHost, int defaultPort = 8080);

    [[nodiscard]] std::vector<ServerManifestEntry> fetchManifest() override;

private:
    std::string serverHost_;
    int defaultPort_;
};

} // namespace musicbox::sync

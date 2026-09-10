#pragma once

#include <cstdint>
#include <string>

namespace musicbox::sync {

struct HttpClientResponse {
    bool connected = false; // false if the TCP connection/DNS lookup itself failed
    int statusCode = 0;
    std::string body;
    std::string error; // populated when connected == false
};

// Minimal blocking HTTP/1.1 client used only by the sync client's Http*
// adapters (HttpServerManifestSource, HttpUploadTransport) to talk to the
// *proposed* /api/v1/sync/* endpoints (docs/api.md). Deliberately simple: one
// request per TCP connection, Content-Length-delimited responses only (no
// chunked transfer-encoding, no TLS, no redirects, no keep-alive, no retries).
//
// This is not a replacement for the real HTTP implementation in server/ (Agent
// 2 owns the actual non-blocking parser/router) — it exists solely so the sync
// client has something concrete to call once the proposed endpoints exist,
// consistent with the project's "implement our own HTTP" stance (Plan.md §2)
// rather than reaching for libcurl for two call sites.
class SimpleHttpClient {
public:
    // host may optionally include ":port" (e.g. "musicbox.local:9000");
    // otherwise defaultPort is used.
    explicit SimpleHttpClient(std::string hostAndMaybePort, int defaultPort = 8080);

    [[nodiscard]] HttpClientResponse get(const std::string& path) const;

    // Sends the contents of absoluteFilePath as the POST body, with headers
    // identifying the track's relative path and content length. See docs/api.md
    // "Proposed: POST /api/v1/sync/tracks".
    [[nodiscard]] HttpClientResponse postFile(const std::string& path, const std::string& relativePath,
                                               const std::string& absoluteFilePath) const;

private:
    std::string host_;
    int port_;

    [[nodiscard]] HttpClientResponse sendRequest(const std::string& requestHead, const char* bodyData,
                                                  std::size_t bodySize) const;
};

} // namespace musicbox::sync

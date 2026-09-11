#include "musicbox/sync/UploadTransport.hpp"

#include "musicbox/sync/HttpClient.hpp"

namespace musicbox::sync {

HttpUploadTransport::HttpUploadTransport(std::string serverHost, int defaultPort)
    : serverHost_(std::move(serverHost)), defaultPort_(defaultPort) {}

bool HttpUploadTransport::uploadFile(const std::string& relativePath,
                                     const std::string& absoluteLocalPath) {
    const SimpleHttpClient client(serverHost_, defaultPort_);

    // Proposed endpoint (docs/api.md, NOT YET IMPLEMENTED server-side):
    //   POST /api/v1/sync/tracks
    //   Headers: X-Relative-Path: <relativePath>, Content-Length: <size>
    //   Body: raw file bytes
    const HttpClientResponse response =
        client.postFile("/api/v1/sync/tracks", relativePath, absoluteLocalPath);

    // Ordinary transfer failures (unreachable server, non-2xx, endpoint not
    // implemented yet) are reported as `false` rather than thrown, so a `push`
    // run can report per-file failures and continue with the rest.
    return response.connected && response.statusCode >= 200 && response.statusCode < 300;
}

} // namespace musicbox::sync

#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "musicbox/http/HttpStatus.hpp"
#include "musicbox/http/ResponseBody.hpp"

namespace musicbox::http {

// A response to send back to the client. Exactly one of `body` / `streamingBody`
// is meaningful for a given response:
//   - small, fully in-memory payloads (JSON, error bodies) use `body`.
//   - large or streamed payloads (audio, artwork) use `streamingBody` and leave
//     `body` empty.
// Move-only: it may own a ResponseBody via unique_ptr.
struct HttpResponse {
    HttpStatus statusCode = HttpStatus::Ok;
    std::unordered_map<std::string, std::string> headers;

    std::vector<std::byte> body;
    std::unique_ptr<ResponseBody> streamingBody;

    HttpResponse() = default;
    HttpResponse(const HttpResponse&) = delete;
    HttpResponse& operator=(const HttpResponse&) = delete;
    HttpResponse(HttpResponse&&) noexcept = default;
    HttpResponse& operator=(HttpResponse&&) noexcept = default;

    [[nodiscard]] bool isStreaming() const noexcept { return streamingBody != nullptr; }

    // Convenience factories used throughout the handlers and tests.
    static HttpResponse json(HttpStatus status, std::string jsonBody);
    static HttpResponse text(HttpStatus status, std::string textBody);
    static HttpResponse error(HttpStatus status, std::string code, std::string message);
};

} // namespace musicbox::http

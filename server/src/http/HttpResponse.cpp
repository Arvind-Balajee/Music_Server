#include "musicbox/http/HttpResponse.hpp"

#include <cstring>

#include <nlohmann/json.hpp>

namespace musicbox::http {

namespace {

[[nodiscard]] std::vector<std::byte> toBytes(const std::string& text) {
    std::vector<std::byte> bytes(text.size());
    if (!text.empty()) {
        std::memcpy(bytes.data(), text.data(), text.size());
    }
    return bytes;
}

[[nodiscard]] HttpResponse makeBodyResponse(HttpStatus status, std::string contentType,
                                             std::string bodyText) {
    HttpResponse response;
    response.statusCode = status;
    response.headers["Content-Type"] = std::move(contentType);
    response.headers["Content-Length"] = std::to_string(bodyText.size());
    response.body = toBytes(bodyText);
    return response;
}

} // namespace

HttpResponse HttpResponse::json(HttpStatus status, std::string jsonBody) {
    return makeBodyResponse(status, "application/json", std::move(jsonBody));
}

HttpResponse HttpResponse::text(HttpStatus status, std::string textBody) {
    return makeBodyResponse(status, "text/plain; charset=utf-8", std::move(textBody));
}

// Error body shape per docs/api.md:
//   { "error": { "code": "TRACK_NOT_FOUND", "message": "..." } }
HttpResponse HttpResponse::error(HttpStatus status, std::string code, std::string message) {
    nlohmann::json body;
    body["error"]["code"] = std::move(code);
    body["error"]["message"] = std::move(message);
    return makeBodyResponse(status, "application/json", body.dump());
}

} // namespace musicbox::http

#pragma once

#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

namespace musicbox::http {

// A fully-parsed HTTP request. Header keys are stored lower-cased so lookups
// don't need to be case-insensitive at every call site.
struct HttpRequest {
    std::string method;   // "GET", "POST", "HEAD", ...
    std::string path;     // decoded, percent-decoded, without the query string
    std::string query;    // raw query string (no leading '?'), "" if none
    std::string version;  // "HTTP/1.1"

    std::unordered_map<std::string, std::string> headers; // lower-cased keys

    std::vector<std::byte> body;

    // Returns the header value if present (case-insensitive lookup against the
    // already-lower-cased keys); caller must lower-case `name` itself.
    [[nodiscard]] const std::string* header(const std::string& lowerCaseName) const {
        auto it = headers.find(lowerCaseName);
        return it == headers.end() ? nullptr : &it->second;
    }
};

} // namespace musicbox::http

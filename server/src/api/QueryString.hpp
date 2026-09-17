#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace musicbox::api {

// HttpRequest::query is kept raw/undecoded by the parser (docs/http.md) --
// this is the API layer's own small decoder, separate from the HTTP parser's
// path percent-decoding (server/src/http/HttpRequestParser.cpp), since query
// strings additionally treat '+' as a space (form-encoding convention) which
// paths must not.
[[nodiscard]] std::unordered_map<std::string, std::string> parseQueryString(std::string_view query);

// Returns the parsed integer if `key` is present in `query` and holds a
// valid base-10 integer; nullopt if the key is simply absent. If `key` is
// present but not a valid integer, sets `*invalid` to true (when non-null)
// so a route handler can distinguish "use the default" from "the client sent
// a malformed value, respond 400" rather than treating both the same way.
[[nodiscard]] std::optional<std::int64_t>
queryInt(const std::unordered_map<std::string, std::string>& query, const std::string& key,
         bool* invalid = nullptr);

} // namespace musicbox::api

#pragma once

#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>

#include "musicbox/http/HttpRequest.hpp"
#include "musicbox/http/HttpResponse.hpp"

namespace musicbox::http {

// Named path segments captured from a route pattern, e.g. pattern
// "/api/v1/tracks/{id}" matched against "/api/v1/tracks/172" yields {"id": "172"}.
using RouteParams = std::unordered_map<std::string, std::string>;

using RequestHandler = std::function<HttpResponse(const HttpRequest&, const RouteParams&)>;

// Dispatches a parsed HttpRequest to the registered handler whose method and path
// pattern match. Implementations own route-matching (see docs/http.md); handlers
// contain no routing logic themselves.
class Router {
public:
    virtual ~Router() = default;

    // `pattern` is a path template such as "/api/v1/tracks/{id}". Registering the
    // same (method, pattern) pair twice is a programming error (implementations
    // may assert/throw).
    virtual void addRoute(std::string method, std::string pattern, RequestHandler handler) = 0;

    // No route matches the path at all -> 404. Path matches but method doesn't
    // -> 405. Otherwise the matched handler's result is returned as-is.
    [[nodiscard]] virtual HttpResponse dispatch(const HttpRequest& request) const = 0;
};

} // namespace musicbox::http

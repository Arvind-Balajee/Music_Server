#pragma once

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

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

// Concrete Router implementation. Route patterns are split into '/'-separated
// segments; a segment written as "{name}" matches any single path segment and
// captures it into RouteParams under "name". No route matches the requested
// path at all -> HttpResponse::error(NotFound, ...); the path matches one or
// more registered routes but none for this method -> error(MethodNotAllowed).
// Registering the same (method, pattern) pair twice throws std::logic_error
// (a programming error, per the base class contract).
class PathRouter final : public Router {
public:
    void addRoute(std::string method, std::string pattern, RequestHandler handler) override;
    [[nodiscard]] HttpResponse dispatch(const HttpRequest& request) const override;

private:
    struct Route {
        std::string method;
        std::vector<std::string> segments; // pattern split on '/'; "{name}" entries are params
        RequestHandler handler;
    };

    std::vector<Route> routes_;
};

// Factory for the default Router implementation, matching the
// open*/create* construction style used elsewhere in server/include.
[[nodiscard]] std::unique_ptr<Router> createRouter();

} // namespace musicbox::http

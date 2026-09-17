#include "musicbox/http/Router.hpp"

#include <stdexcept>

namespace musicbox::http {

namespace {

[[nodiscard]] std::vector<std::string> splitPath(std::string_view path) {
    std::vector<std::string> segments;
    std::size_t i = 0;
    while (i < path.size()) {
        while (i < path.size() && path[i] == '/') {
            ++i;
        }
        const std::size_t start = i;
        while (i < path.size() && path[i] != '/') {
            ++i;
        }
        if (i > start) {
            segments.emplace_back(path.substr(start, i - start));
        }
    }
    return segments;
}

[[nodiscard]] bool isParamSegment(const std::string& segment) {
    return segment.size() >= 2 && segment.front() == '{' && segment.back() == '}';
}

} // namespace

void PathRouter::addRoute(std::string method, std::string pattern, RequestHandler handler) {
    Route route;
    route.method = std::move(method);
    route.segments = splitPath(pattern);
    route.handler = std::move(handler);

    for (const auto& existing : routes_) {
        if (existing.method == route.method && existing.segments == route.segments) {
            throw std::logic_error("Router::addRoute: duplicate route registered for " +
                                   route.method + " " + pattern);
        }
    }

    routes_.push_back(std::move(route));
}

HttpResponse PathRouter::dispatch(const HttpRequest& request) const {
    // HEAD is RFC 7231 §4.3.2's "identical to GET, but the server MUST NOT send
    // a message body" -- a generic HTTP-level policy, not a route-specific
    // concern, so it's handled once here instead of requiring every GET route
    // to also be registered under HEAD. A HEAD request is matched against
    // whatever GET route handles the same path and that handler runs exactly
    // as it would for a GET; the response returned still has its full
    // body/streamingBody populated (headers like Content-Length/Content-Range
    // need to reflect what a GET would have sent). Actually omitting the body
    // bytes on the wire is the transport layer's job -- see
    // writeHttpResponse() in server/src/main.cpp.
    const bool isHeadRequest = request.method == "HEAD";
    const std::vector<std::string> requestSegments = splitPath(request.path);
    bool pathMatchedSomeMethod = false;

    for (const auto& route : routes_) {
        if (route.segments.size() != requestSegments.size()) {
            continue;
        }

        RouteParams params;
        bool matched = true;
        for (std::size_t i = 0; i < route.segments.size(); ++i) {
            const std::string& patternSegment = route.segments[i];
            if (isParamSegment(patternSegment)) {
                params.emplace(patternSegment.substr(1, patternSegment.size() - 2),
                               requestSegments[i]);
            } else if (patternSegment != requestSegments[i]) {
                matched = false;
                break;
            }
        }
        if (!matched) {
            continue;
        }

        pathMatchedSomeMethod = true;
        // A HEAD request only ever matches a GET route, never a literal "HEAD"
        // registration (see this function's opening comment) -- checking
        // isHeadRequest first, rather than `route.method == request.method ||
        // ...`, avoids an insertion-order ambiguity if both happened to be
        // registered for the same path.
        const bool methodMatches =
            isHeadRequest ? route.method == "GET" : route.method == request.method;
        if (methodMatches) {
            return route.handler(request, params);
        }
    }

    if (pathMatchedSomeMethod) {
        return HttpResponse::error(HttpStatus::MethodNotAllowed, "METHOD_NOT_ALLOWED",
                                   "The " + request.method + " method is not supported for " +
                                       request.path + ".");
    }
    return HttpResponse::error(HttpStatus::NotFound, "NOT_FOUND",
                               "No route matches " + request.path + ".");
}

std::unique_ptr<Router> createRouter() {
    return std::make_unique<PathRouter>();
}

} // namespace musicbox::http

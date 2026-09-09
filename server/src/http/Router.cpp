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
        if (route.method == request.method) {
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

std::unique_ptr<Router> createRouter() { return std::make_unique<PathRouter>(); }

} // namespace musicbox::http

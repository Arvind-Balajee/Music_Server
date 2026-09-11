#include <catch2/catch_test_macros.hpp>
#include <stdexcept>

#include "musicbox/http/Router.hpp"

using musicbox::http::createRouter;
using musicbox::http::HttpRequest;
using musicbox::http::HttpResponse;
using musicbox::http::HttpStatus;
using musicbox::http::RouteParams;

namespace {

HttpRequest makeRequest(std::string method, std::string path) {
    HttpRequest request;
    request.method = std::move(method);
    request.path = std::move(path);
    request.version = "HTTP/1.1";
    return request;
}

} // namespace

TEST_CASE("Router matches a {name} path segment and passes it through RouteParams",
          "[http][router]") {
    auto router = createRouter();
    router->addRoute("GET", "/api/v1/tracks/{id}",
                     [](const HttpRequest&, const RouteParams& params) {
                         REQUIRE(params.contains("id"));
                         return HttpResponse::text(HttpStatus::Ok, "track " + params.at("id"));
                     });

    auto response = router->dispatch(makeRequest("GET", "/api/v1/tracks/172"));
    CHECK(response.statusCode == HttpStatus::Ok);
}

TEST_CASE("Router matches multiple named segments independently", "[http][router]") {
    auto router = createRouter();
    router->addRoute("DELETE", "/api/v1/playlists/{id}/tracks/{trackId}",
                     [](const HttpRequest&, const RouteParams& params) {
                         CHECK(params.at("id") == "5");
                         CHECK(params.at("trackId") == "42");
                         return HttpResponse::text(HttpStatus::Ok, "ok");
                     });

    auto response = router->dispatch(makeRequest("DELETE", "/api/v1/playlists/5/tracks/42"));
    CHECK(response.statusCode == HttpStatus::Ok);
}

TEST_CASE("Router returns 404 when no route matches the path", "[http][router]") {
    auto router = createRouter();
    router->addRoute("GET", "/api/v1/tracks", [](const HttpRequest&, const RouteParams&) {
        return HttpResponse::text(HttpStatus::Ok, "ok");
    });

    auto response = router->dispatch(makeRequest("GET", "/api/v1/nonexistent"));
    CHECK(response.statusCode == HttpStatus::NotFound);
}

TEST_CASE("Router returns 405 when the path matches but the method does not", "[http][router]") {
    auto router = createRouter();
    router->addRoute("GET", "/api/v1/tracks/{id}", [](const HttpRequest&, const RouteParams&) {
        return HttpResponse::text(HttpStatus::Ok, "ok");
    });

    auto response = router->dispatch(makeRequest("DELETE", "/api/v1/tracks/172"));
    CHECK(response.statusCode == HttpStatus::MethodNotAllowed);
}

TEST_CASE("Router requires exact segment counts (no partial prefix match)", "[http][router]") {
    auto router = createRouter();
    router->addRoute("GET", "/api/v1/tracks/{id}", [](const HttpRequest&, const RouteParams&) {
        return HttpResponse::text(HttpStatus::Ok, "ok");
    });

    auto response = router->dispatch(makeRequest("GET", "/api/v1/tracks/172/stream"));
    CHECK(response.statusCode == HttpStatus::NotFound);
}

TEST_CASE("Router distinguishes static segments from param segments", "[http][router]") {
    auto router = createRouter();
    router->addRoute("GET", "/api/v1/tracks/count", [](const HttpRequest&, const RouteParams&) {
        return HttpResponse::text(HttpStatus::Ok, "count-route");
    });
    router->addRoute("GET", "/api/v1/tracks/{id}",
                     [](const HttpRequest&, const RouteParams& params) {
                         return HttpResponse::text(HttpStatus::Ok, "id-route:" + params.at("id"));
                     });

    auto countResponse = router->dispatch(makeRequest("GET", "/api/v1/tracks/count"));
    CHECK(countResponse.statusCode == HttpStatus::Ok);

    auto idResponse = router->dispatch(makeRequest("GET", "/api/v1/tracks/42"));
    CHECK(idResponse.statusCode == HttpStatus::Ok);
}

TEST_CASE("Router::addRoute throws on a duplicate (method, pattern) registration",
          "[http][router]") {
    auto router = createRouter();
    router->addRoute("GET", "/x", [](const HttpRequest&, const RouteParams&) {
        return HttpResponse::text(HttpStatus::Ok, "ok");
    });
    CHECK_THROWS_AS(router->addRoute("GET", "/x",
                                     [](const HttpRequest&, const RouteParams&) {
                                         return HttpResponse::text(HttpStatus::Ok, "ok");
                                     }),
                    std::logic_error);
}

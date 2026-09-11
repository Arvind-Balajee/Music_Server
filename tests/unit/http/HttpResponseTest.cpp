#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>
#include <string>

#include "musicbox/http/HttpResponse.hpp"

using musicbox::http::HttpResponse;
using musicbox::http::HttpStatus;

namespace {
std::string bodyAsString(const HttpResponse& response) {
    return std::string(reinterpret_cast<const char*>(response.body.data()), response.body.size());
}
} // namespace

TEST_CASE("HttpResponse::json sets Content-Type and Content-Length", "[http][response]") {
    auto response = HttpResponse::json(HttpStatus::Ok, R"({"status":"ok"})");
    CHECK(response.statusCode == HttpStatus::Ok);
    CHECK(response.headers.at("Content-Type") == "application/json");
    CHECK(response.headers.at("Content-Length") == std::to_string(bodyAsString(response).size()));
    CHECK(bodyAsString(response) == R"({"status":"ok"})");
}

TEST_CASE("HttpResponse::text sets a text Content-Type", "[http][response]") {
    auto response = HttpResponse::text(HttpStatus::Ok, "MusicBox");
    CHECK(bodyAsString(response) == "MusicBox");
    CHECK(response.headers.at("Content-Type").starts_with("text/plain"));
}

TEST_CASE("HttpResponse::error matches the docs/api.md error shape", "[http][response]") {
    auto response = HttpResponse::error(HttpStatus::NotFound, "TRACK_NOT_FOUND",
                                        "The requested track does not exist.");
    CHECK(response.statusCode == HttpStatus::NotFound);

    auto parsed = nlohmann::json::parse(bodyAsString(response));
    REQUIRE(parsed.contains("error"));
    CHECK(parsed["error"]["code"] == "TRACK_NOT_FOUND");
    CHECK(parsed["error"]["message"] == "The requested track does not exist.");
}

#pragma once

namespace musicbox::http {

// Status codes required for the MVP (Plan.md §8). Extend additively.
enum class HttpStatus : int {
    Ok = 200,
    PartialContent = 206,
    BadRequest = 400,
    NotFound = 404,
    MethodNotAllowed = 405,
    RangeNotSatisfiable = 416,
    InternalServerError = 500,
};

// Human-readable reason phrase for the status line (e.g. "200 OK").
[[nodiscard]] const char* reasonPhrase(HttpStatus status) noexcept;

} // namespace musicbox::http

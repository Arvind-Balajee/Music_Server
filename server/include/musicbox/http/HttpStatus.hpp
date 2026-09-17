#pragma once

namespace musicbox::http {

// Status codes required for the MVP (Plan.md §8), extended additively by
// Agent 4 (API layer, docs/api.md) with the two REST-conventional codes the
// playlist CRUD endpoints need that weren't in Agent 2's original streaming-
// focused list.
enum class HttpStatus : int {
    Ok = 200,
    Created = 201,   // POST /api/v1/playlists
    NoContent = 204, // DELETE endpoints, POST .../tracks, DELETE .../tracks/{id}
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

import Foundation

/// The `{ "error": { "code", "message" } }` envelope from `docs/api.md`.
struct APIErrorEnvelope: Codable, Sendable {
    let error: APIErrorBody
}

struct APIErrorBody: Codable, Hashable, Sendable {
    let code: String
    let message: String
}

/// Stable error codes from `docs/api.md`. The client switches on `code` (a
/// `SCREAMING_SNAKE_CASE` string per the API contract) rather than parsing
/// `message`; unrecognized codes decode to `.other` so the client tolerates the
/// server adding new codes later.
enum APIErrorCode: String, Sendable {
    case trackNotFound = "TRACK_NOT_FOUND"
    case albumNotFound = "ALBUM_NOT_FOUND"
    case artistNotFound = "ARTIST_NOT_FOUND"
    case playlistNotFound = "PLAYLIST_NOT_FOUND"
    case badRequest = "BAD_REQUEST"
    case rangeNotSatisfiable = "RANGE_NOT_SATISFIABLE"
    case validationError = "VALIDATION_ERROR"
    case internalError = "INTERNAL_ERROR"
}

/// Errors surfaced by `MusicBoxAPIClient` implementations to Repositories/ViewModels.
enum MusicBoxAPIError: Error, LocalizedError, Sendable {
    /// The server responded with a well-formed `{ "error": ... }` envelope.
    case server(code: String, message: String, httpStatus: Int)
    /// A non-2xx response without a parseable error envelope.
    case unexpectedStatus(Int)
    /// The response body couldn't be decoded into the expected model.
    case decoding(String)
    /// `URLSession` itself failed (offline, timeout, connection refused, ...).
    case transport(String)
    /// The configured base URL / server address is missing or malformed.
    case invalidServerAddress

    var errorDescription: String? {
        switch self {
        case .server(let code, let message, _):
            return "\(message) (\(code))"
        case .unexpectedStatus(let status):
            return "Unexpected server response (HTTP \(status))."
        case .decoding:
            return "The server response could not be understood."
        case .transport(let message):
            return "Network error: \(message)"
        case .invalidServerAddress:
            return "No MusicBox server is configured. Check Settings."
        }
    }

    var code: APIErrorCode? {
        if case .server(let code, _, _) = self {
            return APIErrorCode(rawValue: code)
        }
        return nil
    }
}

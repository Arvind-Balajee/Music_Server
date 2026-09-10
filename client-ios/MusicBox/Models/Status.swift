import Foundation

/// `GET /api/v1/status` — matches `docs/api.md` exactly.
struct StatusResponse: Codable, Hashable, Sendable {
    let status: String
    let version: String
    let trackCount: Int
    let libraryRoots: Int
}

/// `GET /api/v1/search?q=` response shape. Not specified in `docs/api.md` yet;
/// this client assumes a fan-out across all three entity kinds, matching how
/// `TrackQuery.search` is described in `docs/database.md` ("matches title, artist,
/// album"). Confirm against Agent 4's real implementation once it lands.
struct SearchResults: Codable, Hashable, Sendable {
    let artists: [Artist]
    let albums: [Album]
    let tracks: [Track]

    static let empty = SearchResults(artists: [], albums: [], tracks: [])
}

import Foundation

/// Summary shape for `GET /api/v1/playlists` list entries. `docs/database.md`
/// defines `Playlist { id, name, createdAt, updatedAt }`; `trackCount` is this
/// client's assumption for what a list view needs and isn't confirmed against the
/// real API yet (see docs/ios.md open questions).
struct Playlist: Codable, Identifiable, Hashable, Sendable {
    let id: PlaylistId
    let name: String
    let createdAt: String?
    let updatedAt: String?
    let trackCount: Int?

    init(
        id: PlaylistId,
        name: String,
        createdAt: String? = nil,
        updatedAt: String? = nil,
        trackCount: Int? = nil
    ) {
        self.id = id
        self.name = name
        self.createdAt = createdAt
        self.updatedAt = updatedAt
        self.trackCount = trackCount
    }
}

/// `GET /api/v1/playlists/{id}` — assumed to embed the full ordered track list.
/// Not confirmed against the real API yet; see docs/ios.md.
struct PlaylistDetail: Codable, Identifiable, Hashable, Sendable {
    let id: PlaylistId
    let name: String
    let createdAt: String?
    let updatedAt: String?
    let tracks: [Track]

    var summary: Playlist {
        Playlist(id: id, name: name, createdAt: createdAt, updatedAt: updatedAt, trackCount: tracks.count)
    }
}

/// Body for `POST /api/v1/playlists` and `PUT /api/v1/playlists/{id}`.
struct PlaylistUpsertRequest: Codable, Sendable {
    let name: String
}

/// Body for `POST /api/v1/playlists/{id}/tracks`.
struct AddPlaylistTrackRequest: Codable, Sendable {
    let trackId: TrackId
}

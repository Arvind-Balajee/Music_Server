import Foundation

/// Matches the `Album` shape in `docs/database.md`: `{ id, title, albumArtistId?,
/// year?, hasArtwork }`. `docs/api.md` does not spell out the exact serialized
/// album JSON, so — mirroring how `Track` embeds a compact `artist` object rather
/// than a bare `artistId` — this client assumes `GET /api/v1/albums` /
/// `/albums/{id}` embed the album artist as `artist: { id, name }` rather than a
/// raw `albumArtistId` integer. This is a documented assumption (see docs/ios.md)
/// to confirm against Agent 4's real implementation once it lands.
struct Album: Codable, Identifiable, Hashable, Sendable {
    let id: AlbumId
    let title: String
    let artist: ArtistRef?
    let year: Int?
    let hasArtwork: Bool
    let trackCount: Int?

    init(
        id: AlbumId,
        title: String,
        artist: ArtistRef? = nil,
        year: Int? = nil,
        hasArtwork: Bool = false,
        trackCount: Int? = nil
    ) {
        self.id = id
        self.title = title
        self.artist = artist
        self.year = year
        self.hasArtwork = hasArtwork
        self.trackCount = trackCount
    }
}

/// The compact `{ "id", "title" }` reference embedded inside a `Track` response.
struct AlbumRef: Codable, Identifiable, Hashable, Sendable {
    let id: AlbumId
    let title: String
}

extension Album {
    var asRef: AlbumRef { AlbumRef(id: id, title: title) }
}

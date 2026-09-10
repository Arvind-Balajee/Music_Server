import Foundation

/// Matches the `GET /api/v1/tracks/{id}` example in `docs/api.md` for the core
/// fields (`id`, `title`, `artist`, `durationMs`, `streamUrl`). Note `Plan.md` §25's
/// mock JSON example omits `trackNumber`/`codec`/`fileSize` entirely while
/// `docs/api.md`'s fuller example includes them — since the two documented
/// examples disagree on which fields are always present, everything except the
/// fields every screen actually needs to function (id/title/artist/durationMs/
/// streamUrl) is declared optional here so decoding degrades gracefully either
/// way; see `ModelDecodingTests` for both examples decoding successfully.
struct Track: Codable, Identifiable, Hashable, Sendable {
    let id: TrackId
    let title: String
    let artist: ArtistRef
    let album: AlbumRef?
    let trackNumber: Int?
    let discNumber: Int?
    let durationMs: Int
    let codec: String?
    let bitrateKbps: Int?
    let fileSize: Int?
    let genre: String?
    let hasArtwork: Bool?
    let streamUrl: String

    init(
        id: TrackId,
        title: String,
        artist: ArtistRef,
        album: AlbumRef? = nil,
        trackNumber: Int? = nil,
        discNumber: Int? = nil,
        durationMs: Int,
        codec: String? = nil,
        bitrateKbps: Int? = nil,
        fileSize: Int? = nil,
        genre: String? = nil,
        hasArtwork: Bool? = nil,
        streamUrl: String
    ) {
        self.id = id
        self.title = title
        self.artist = artist
        self.album = album
        self.trackNumber = trackNumber
        self.discNumber = discNumber
        self.durationMs = durationMs
        self.codec = codec
        self.bitrateKbps = bitrateKbps
        self.fileSize = fileSize
        self.genre = genre
        self.hasArtwork = hasArtwork
        self.streamUrl = streamUrl
    }

    var duration: TimeInterval { TimeInterval(durationMs) / 1000.0 }
}

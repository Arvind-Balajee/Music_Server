import Foundation

/// The single seam between this app and the MusicBox server. Every endpoint in
/// `docs/api.md` has a corresponding method here. Two conformances exist:
/// `LiveMusicBoxAPIClient` (real `URLSession` networking) and
/// `MockMusicBoxAPIClient` (in-memory fixture data per `Plan.md` §25), so the rest
/// of the app — Repositories, ViewModels, Views — never knows or cares which one
/// it's talking to.
///
/// Architecture rule (Plan.md §11): no networking code belongs in a SwiftUI View.
/// Views only ever see ViewModels; ViewModels only ever see Repositories;
/// Repositories are the only callers of `MusicBoxAPIClient`.
protocol MusicBoxAPIClient: Sendable {
    /// The server's base URL (e.g. `http://musicbox.local:8080`), used to resolve
    /// the relative `streamUrl` paths embedded in `Track` responses.
    var baseURL: URL { get }

    func status() async throws -> StatusResponse

    func artists(limit: Int, offset: Int) async throws -> [Artist]
    func artist(id: ArtistId) async throws -> Artist

    func albums(artistId: ArtistId?, limit: Int, offset: Int) async throws -> [Album]
    func album(id: AlbumId) async throws -> Album

    func tracks(artistId: ArtistId?, albumId: AlbumId?, search: String?, limit: Int, offset: Int) async throws -> [Track]
    func track(id: TrackId) async throws -> Track

    func search(query: String) async throws -> SearchResults

    func playlists() async throws -> [Playlist]
    func playlist(id: PlaylistId) async throws -> PlaylistDetail
    func createPlaylist(name: String) async throws -> PlaylistDetail
    func updatePlaylist(id: PlaylistId, name: String) async throws -> PlaylistDetail
    func deletePlaylist(id: PlaylistId) async throws
    func addTrack(_ trackId: TrackId, toPlaylist playlistId: PlaylistId) async throws
    func removeTrack(_ trackId: TrackId, fromPlaylist playlistId: PlaylistId) async throws
}

/// Convenience overloads with the common defaults so call sites don't repeat
/// `limit`/`offset`/`nil` filters everywhere. Protocol requirements themselves
/// can't carry default argument values, so these live in an extension instead.
extension MusicBoxAPIClient {
    /// `GET /api/v1/tracks/{id}/stream` — resolved client-side from `baseURL`
    /// plus the track id, mirroring how the server documents the route (rather
    /// than trusting the `streamUrl` string verbatim, though today they agree).
    func streamURL(for trackId: TrackId) -> URL {
        baseURL.appendingPathComponent("api/v1/tracks/\(trackId)/stream")
    }

    /// `GET /api/v1/tracks/{id}/artwork`.
    func artworkURL(for trackId: TrackId) -> URL {
        baseURL.appendingPathComponent("api/v1/tracks/\(trackId)/artwork")
    }

    func artists() async throws -> [Artist] {
        try await artists(limit: 200, offset: 0)
    }

    func albums() async throws -> [Album] {
        try await albums(artistId: nil, limit: 200, offset: 0)
    }

    func albums(byArtist artistId: ArtistId) async throws -> [Album] {
        try await albums(artistId: artistId, limit: 200, offset: 0)
    }

    func tracks() async throws -> [Track] {
        try await tracks(artistId: nil, albumId: nil, search: nil, limit: 500, offset: 0)
    }

    func tracks(byAlbum albumId: AlbumId) async throws -> [Track] {
        try await tracks(artistId: nil, albumId: albumId, search: nil, limit: 500, offset: 0)
    }

    func tracks(byArtist artistId: ArtistId) async throws -> [Track] {
        try await tracks(artistId: artistId, albumId: nil, search: nil, limit: 500, offset: 0)
    }
}

import Foundation

/// Thin seam between ViewModels and `MusicBoxAPIClient` — see `ArtistRepository`
/// for the rationale (future on-device cache insertion point).
struct PlaylistRepository {
    private let client: MusicBoxAPIClient

    init(client: MusicBoxAPIClient) {
        self.client = client
    }

    func listPlaylists() async throws -> [Playlist] {
        try await client.playlists()
    }

    func playlist(id: PlaylistId) async throws -> PlaylistDetail {
        try await client.playlist(id: id)
    }

    func createPlaylist(name: String) async throws -> PlaylistDetail {
        try await client.createPlaylist(name: name)
    }

    func renamePlaylist(id: PlaylistId, name: String) async throws -> PlaylistDetail {
        try await client.updatePlaylist(id: id, name: name)
    }

    func deletePlaylist(id: PlaylistId) async throws {
        try await client.deletePlaylist(id: id)
    }

    func addTrack(_ trackId: TrackId, toPlaylist playlistId: PlaylistId) async throws {
        try await client.addTrack(trackId, toPlaylist: playlistId)
    }

    func removeTrack(_ trackId: TrackId, fromPlaylist playlistId: PlaylistId) async throws {
        try await client.removeTrack(trackId, fromPlaylist: playlistId)
    }
}

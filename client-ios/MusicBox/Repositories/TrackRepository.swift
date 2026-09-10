import Foundation

/// Thin seam between ViewModels and `MusicBoxAPIClient` — see `ArtistRepository`
/// for the rationale (future on-device cache insertion point).
struct TrackRepository {
    private let client: MusicBoxAPIClient

    init(client: MusicBoxAPIClient) {
        self.client = client
    }

    func listTracks() async throws -> [Track] {
        try await client.tracks()
    }

    func listTracks(byAlbum albumId: AlbumId) async throws -> [Track] {
        try await client.tracks(byAlbum: albumId)
    }

    func listTracks(byArtist artistId: ArtistId) async throws -> [Track] {
        try await client.tracks(byArtist: artistId)
    }

    func track(id: TrackId) async throws -> Track {
        try await client.track(id: id)
    }

    /// Resolved absolute URL for streaming a track's audio, for the
    /// `PlaybackController` to hand to `AVPlayer`.
    func streamURL(for trackId: TrackId) -> URL {
        client.streamURL(for: trackId)
    }

    func artworkURL(for trackId: TrackId) -> URL {
        client.artworkURL(for: trackId)
    }
}

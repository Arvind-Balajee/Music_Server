import Foundation

/// Thin seam between ViewModels and `MusicBoxAPIClient` — see `ArtistRepository`
/// for the rationale (future on-device cache insertion point).
struct AlbumRepository {
    private let client: MusicBoxAPIClient

    init(client: MusicBoxAPIClient) {
        self.client = client
    }

    func listAlbums() async throws -> [Album] {
        try await client.albums()
    }

    func listAlbums(byArtist artistId: ArtistId) async throws -> [Album] {
        try await client.albums(byArtist: artistId)
    }

    func album(id: AlbumId) async throws -> Album {
        try await client.album(id: id)
    }
}

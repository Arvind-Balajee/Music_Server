import Foundation

/// Thin seam between ViewModels and `MusicBoxAPIClient`. Intentionally does
/// nothing but forward today — this is where an on-device cache (per `Plan.md`
/// §11's bounded LRU cache discussion) would be inserted later without touching
/// any ViewModel or View code.
struct ArtistRepository {
    private let client: MusicBoxAPIClient

    init(client: MusicBoxAPIClient) {
        self.client = client
    }

    func listArtists() async throws -> [Artist] {
        try await client.artists()
    }

    func artist(id: ArtistId) async throws -> Artist {
        try await client.artist(id: id)
    }
}

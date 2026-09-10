import Foundation

/// Thin seam between ViewModels and `MusicBoxAPIClient` — see `ArtistRepository`
/// for the rationale (future on-device cache insertion point).
struct SearchRepository {
    private let client: MusicBoxAPIClient

    init(client: MusicBoxAPIClient) {
        self.client = client
    }

    func search(query: String) async throws -> SearchResults {
        try await client.search(query: query)
    }
}

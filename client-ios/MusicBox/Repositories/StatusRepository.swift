import Foundation

/// Thin seam between ViewModels and `MusicBoxAPIClient` — see `ArtistRepository`
/// for the rationale (future on-device cache insertion point). Backs the Home and
/// Settings screens' "connected to <server>" status line.
struct StatusRepository {
    private let client: MusicBoxAPIClient

    init(client: MusicBoxAPIClient) {
        self.client = client
    }

    func status() async throws -> StatusResponse {
        try await client.status()
    }
}

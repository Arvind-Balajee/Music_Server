import Foundation
import Combine

/// Small mutable box so `PlaybackController`'s `streamURLProvider` closure can
/// always resolve through the *current* `MusicBoxAPIClient` without capturing
/// `self` from inside `AppEnvironment.init` (illegal in Swift before all stored
/// properties are set) and without `PlaybackController` needing to know about
/// `AppEnvironment`, `SettingsStore`, or the mock/live distinction at all.
private final class APIClientBox: @unchecked Sendable {
    var client: MusicBoxAPIClient
    init(_ client: MusicBoxAPIClient) { self.client = client }
}

/// The app's single composition root / dependency-injection container. Owns the
/// persisted `SettingsStore`, the current `MusicBoxAPIClient` (mock by default —
/// Plan.md §25), the shared `PlaybackController`, and factory properties for each
/// Repository. Views receive this via `.environmentObject` and construct their
/// ViewModels from its repositories — Views never talk to `MusicBoxAPIClient`
/// directly (Plan.md §11 architecture rule).
@MainActor
final class AppEnvironment: ObservableObject {
    let settings: SettingsStore
    @Published private(set) var apiClient: MusicBoxAPIClient
    let playback: PlaybackController

    private let clientBox: APIClientBox
    private var cancellables = Set<AnyCancellable>()

    init(settings: SettingsStore = SettingsStore()) {
        self.settings = settings

        let initialClient = AppEnvironment.buildClient(settings: settings)
        let box = APIClientBox(initialClient)
        self.clientBox = box
        self.apiClient = initialClient
        self.playback = PlaybackController(streamURLProvider: { trackId in box.client.streamURL(for: trackId) })

        // Re-resolve the client whenever Settings changes (mock/live toggle or
        // server address edits). The initial values are applied above already,
        // so `dropFirst` avoids an immediate redundant rebuild.
        Publishers.CombineLatest3(settings.$useMockData, settings.$serverHost, settings.$serverPort)
            .dropFirst()
            .receive(on: DispatchQueue.main)
            .sink { [weak self] _, _, _ in
                self?.rebuildClient()
            }
            .store(in: &cancellables)
    }

    private func rebuildClient() {
        let newClient = AppEnvironment.buildClient(settings: settings)
        clientBox.client = newClient
        apiClient = newClient
    }

    private static func buildClient(settings: SettingsStore) -> MusicBoxAPIClient {
        guard !settings.useMockData else { return MockMusicBoxAPIClient() }
        guard let baseURL = settings.resolvedBaseURL else {
            // No valid server configured yet — fall back to the mock rather than
            // handing out a client that can't build a single URL.
            return MockMusicBoxAPIClient()
        }
        return LiveMusicBoxAPIClient(baseURL: baseURL)
    }

    // MARK: - Repositories (constructed fresh against the current client; see
    // ArtistRepository's doc comment for why these are intentionally thin)

    var artistRepository: ArtistRepository { ArtistRepository(client: apiClient) }
    var albumRepository: AlbumRepository { AlbumRepository(client: apiClient) }
    var trackRepository: TrackRepository { TrackRepository(client: apiClient) }
    var playlistRepository: PlaylistRepository { PlaylistRepository(client: apiClient) }
    var searchRepository: SearchRepository { SearchRepository(client: apiClient) }
    var statusRepository: StatusRepository { StatusRepository(client: apiClient) }
}

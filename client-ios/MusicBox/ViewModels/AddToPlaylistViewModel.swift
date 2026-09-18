import Foundation

/// Backs `AddToPlaylistSheet`: lists existing playlists and adds `track` to
/// whichever one the user taps, creating a new playlist inline if needed.
@MainActor
final class AddToPlaylistViewModel: ObservableObject {
    let track: Track

    @Published private(set) var state: LoadState<[Playlist]> = .idle
    @Published private(set) var addedPlaylistIds: Set<PlaylistId> = []
    @Published var errorBanner: String?

    private let repository: PlaylistRepository

    init(track: Track, repository: PlaylistRepository) {
        self.track = track
        self.repository = repository
    }

    func load() async {
        state = .loading
        do {
            state = .loaded(try await repository.listPlaylists())
        } catch {
            state = .failed(error.localizedDescription)
        }
    }

    func add(to playlist: Playlist) async {
        do {
            try await repository.addTrack(track.id, toPlaylist: playlist.id)
            addedPlaylistIds.insert(playlist.id)
        } catch {
            errorBanner = error.localizedDescription
        }
    }

    func createPlaylistAndAdd(named name: String) async {
        let trimmed = name.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !trimmed.isEmpty else { return }
        do {
            let created = try await repository.createPlaylist(name: trimmed)
            try await repository.addTrack(track.id, toPlaylist: created.id)
            addedPlaylistIds.insert(created.id)
            await load()
        } catch {
            errorBanner = error.localizedDescription
        }
    }
}

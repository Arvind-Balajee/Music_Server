import Foundation

@MainActor
final class PlaylistDetailViewModel: ObservableObject {
    let playlistId: PlaylistId

    @Published private(set) var state: LoadState<PlaylistDetail> = .idle
    @Published var errorBanner: String?

    private let repository: PlaylistRepository

    init(playlistId: PlaylistId, repository: PlaylistRepository) {
        self.playlistId = playlistId
        self.repository = repository
    }

    func load() async {
        state = .loading
        do {
            state = .loaded(try await repository.playlist(id: playlistId))
        } catch {
            state = .failed(error.localizedDescription)
        }
    }

    func removeTrack(_ trackId: TrackId) async {
        do {
            try await repository.removeTrack(trackId, fromPlaylist: playlistId)
            await load()
        } catch {
            errorBanner = error.localizedDescription
        }
    }

    func rename(to name: String) async {
        let trimmed = name.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !trimmed.isEmpty else { return }
        do {
            _ = try await repository.renamePlaylist(id: playlistId, name: trimmed)
            await load()
        } catch {
            errorBanner = error.localizedDescription
        }
    }
}

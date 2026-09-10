import Foundation

@MainActor
final class PlaylistsViewModel: ObservableObject {
    @Published private(set) var state: LoadState<[Playlist]> = .idle
    @Published var errorBanner: String?

    private let repository: PlaylistRepository

    init(repository: PlaylistRepository) {
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

    func createPlaylist(named name: String) async {
        let trimmed = name.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !trimmed.isEmpty else { return }
        do {
            _ = try await repository.createPlaylist(name: trimmed)
            await load()
        } catch {
            errorBanner = error.localizedDescription
        }
    }

    func deletePlaylist(id: PlaylistId) async {
        do {
            try await repository.deletePlaylist(id: id)
            await load()
        } catch {
            errorBanner = error.localizedDescription
        }
    }
}

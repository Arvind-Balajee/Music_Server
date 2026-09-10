import Foundation

@MainActor
final class AlbumsViewModel: ObservableObject {
    @Published private(set) var state: LoadState<[Album]> = .idle

    private let repository: AlbumRepository

    init(repository: AlbumRepository) {
        self.repository = repository
    }

    func load() async {
        state = .loading
        do {
            state = .loaded(try await repository.listAlbums())
        } catch {
            state = .failed(error.localizedDescription)
        }
    }
}

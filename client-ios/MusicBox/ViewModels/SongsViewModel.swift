import Foundation

@MainActor
final class SongsViewModel: ObservableObject {
    @Published private(set) var state: LoadState<[Track]> = .idle

    private let repository: TrackRepository

    init(repository: TrackRepository) {
        self.repository = repository
    }

    func load() async {
        state = .loading
        do {
            state = .loaded(try await repository.listTracks())
        } catch {
            state = .failed(error.localizedDescription)
        }
    }
}

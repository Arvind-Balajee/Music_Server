import Foundation

@MainActor
final class ArtistsViewModel: ObservableObject {
    @Published private(set) var state: LoadState<[Artist]> = .idle

    private let repository: ArtistRepository

    init(repository: ArtistRepository) {
        self.repository = repository
    }

    func load() async {
        state = .loading
        do {
            state = .loaded(try await repository.listArtists())
        } catch {
            state = .failed(error.localizedDescription)
        }
    }
}

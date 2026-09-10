import Foundation

@MainActor
final class HomeViewModel: ObservableObject {
    @Published private(set) var status: LoadState<StatusResponse> = .idle
    @Published private(set) var recentAlbums: LoadState<[Album]> = .idle

    private let statusRepository: StatusRepository
    private let albumRepository: AlbumRepository

    init(statusRepository: StatusRepository, albumRepository: AlbumRepository) {
        self.statusRepository = statusRepository
        self.albumRepository = albumRepository
    }

    func load() async {
        status = .loading
        recentAlbums = .loading
        async let statusResult = statusRepository.status()
        async let albumsResult = albumRepository.listAlbums()

        do {
            status = .loaded(try await statusResult)
        } catch {
            status = .failed(error.localizedDescription)
        }
        do {
            recentAlbums = .loaded(Array(try await albumsResult.prefix(6)))
        } catch {
            recentAlbums = .failed(error.localizedDescription)
        }
    }
}

import Foundation

@MainActor
final class AlbumDetailViewModel: ObservableObject {
    let albumId: AlbumId

    @Published private(set) var album: LoadState<Album> = .idle
    @Published private(set) var tracks: LoadState<[Track]> = .idle

    private let albumRepository: AlbumRepository
    private let trackRepository: TrackRepository

    init(albumId: AlbumId, albumRepository: AlbumRepository, trackRepository: TrackRepository) {
        self.albumId = albumId
        self.albumRepository = albumRepository
        self.trackRepository = trackRepository
    }

    func load() async {
        album = .loading
        tracks = .loading
        async let albumResult = albumRepository.album(id: albumId)
        async let tracksResult = trackRepository.listTracks(byAlbum: albumId)

        do {
            album = .loaded(try await albumResult)
        } catch {
            album = .failed(error.localizedDescription)
        }
        do {
            let sorted = try await tracksResult.sorted {
                ($0.trackNumber ?? Int.max) < ($1.trackNumber ?? Int.max)
            }
            tracks = .loaded(sorted)
        } catch {
            tracks = .failed(error.localizedDescription)
        }
    }
}

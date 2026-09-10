import Foundation

@MainActor
final class ArtistDetailViewModel: ObservableObject {
    let artistId: ArtistId

    @Published private(set) var artist: LoadState<Artist> = .idle
    @Published private(set) var albums: LoadState<[Album]> = .idle

    private let artistRepository: ArtistRepository
    private let albumRepository: AlbumRepository

    init(artistId: ArtistId, artistRepository: ArtistRepository, albumRepository: AlbumRepository) {
        self.artistId = artistId
        self.artistRepository = artistRepository
        self.albumRepository = albumRepository
    }

    func load() async {
        artist = .loading
        albums = .loading
        async let artistResult = artistRepository.artist(id: artistId)
        async let albumsResult = albumRepository.listAlbums(byArtist: artistId)

        do {
            artist = .loaded(try await artistResult)
        } catch {
            artist = .failed(error.localizedDescription)
        }
        do {
            albums = .loaded(try await albumsResult)
        } catch {
            albums = .failed(error.localizedDescription)
        }
    }
}

import SwiftUI

struct ArtistsView: View {
    @EnvironmentObject private var environment: AppEnvironment
    @StateObject private var viewModel: ArtistsViewModel

    init(viewModel: @autoclosure @escaping () -> ArtistsViewModel) {
        _viewModel = StateObject(wrappedValue: viewModel())
    }

    var body: some View {
        LoadStateView(state: viewModel.state) { artists in
            List(artists) { artist in
                NavigationLink(value: artist) {
                    ArtistRow(artist: artist)
                }
            }
        }
        .navigationDestination(for: Artist.self) { artist in
            ArtistDetailView(viewModel: ArtistDetailViewModel(
                artistId: artist.id,
                artistRepository: environment.artistRepository,
                albumRepository: environment.albumRepository
            ))
        }
        .navigationDestination(for: Album.self) { album in
            AlbumDetailView(viewModel: AlbumDetailViewModel(
                albumId: album.id,
                albumRepository: environment.albumRepository,
                trackRepository: environment.trackRepository
            ))
        }
        .navigationTitle("Artists")
        .task { await viewModel.load() }
        .refreshable { await viewModel.load() }
    }
}

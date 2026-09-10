import SwiftUI

struct AlbumsView: View {
    @EnvironmentObject private var environment: AppEnvironment
    @StateObject private var viewModel: AlbumsViewModel

    init(viewModel: @autoclosure @escaping () -> AlbumsViewModel) {
        _viewModel = StateObject(wrappedValue: viewModel())
    }

    var body: some View {
        LoadStateView(state: viewModel.state) { albums in
            List(albums) { album in
                NavigationLink(value: album) {
                    AlbumRow(album: album)
                }
            }
        }
        .navigationDestination(for: Album.self) { album in
            AlbumDetailView(viewModel: AlbumDetailViewModel(
                albumId: album.id,
                albumRepository: environment.albumRepository,
                trackRepository: environment.trackRepository
            ))
        }
        .navigationTitle("Albums")
        .task { await viewModel.load() }
        .refreshable { await viewModel.load() }
    }
}

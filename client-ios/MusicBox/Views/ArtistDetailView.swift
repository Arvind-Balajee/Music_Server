import SwiftUI

struct ArtistDetailView: View {
    @StateObject private var viewModel: ArtistDetailViewModel

    init(viewModel: @autoclosure @escaping () -> ArtistDetailViewModel) {
        _viewModel = StateObject(wrappedValue: viewModel())
    }

    var body: some View {
        List {
            LoadStateView(state: viewModel.albums) { albums in
                ForEach(albums) { album in
                    NavigationLink(value: album) {
                        AlbumRow(album: album)
                    }
                }
            }
        }
        .navigationTitle(viewModel.artist.value?.name ?? "Artist")
        .task { await viewModel.load() }
        .refreshable { await viewModel.load() }
    }
}

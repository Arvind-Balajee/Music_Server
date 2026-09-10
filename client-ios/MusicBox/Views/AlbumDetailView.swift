import SwiftUI

struct AlbumDetailView: View {
    @EnvironmentObject private var environment: AppEnvironment
    @StateObject private var viewModel: AlbumDetailViewModel

    init(viewModel: @autoclosure @escaping () -> AlbumDetailViewModel) {
        _viewModel = StateObject(wrappedValue: viewModel())
    }

    var body: some View {
        List {
            LoadStateView(state: viewModel.tracks) { tracks in
                ForEach(tracks) { track in
                    TrackRow(track: track, showAlbum: false) {
                        environment.playback.play(tracks: tracks, startIndex: tracks.firstIndex(of: track) ?? 0)
                    }
                }
            }
        }
        .navigationTitle(viewModel.album.value?.title ?? "Album")
        .task { await viewModel.load() }
        .refreshable { await viewModel.load() }
    }
}

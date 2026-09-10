import SwiftUI

struct SongsView: View {
    @EnvironmentObject private var environment: AppEnvironment
    @StateObject private var viewModel: SongsViewModel

    init(viewModel: @autoclosure @escaping () -> SongsViewModel) {
        _viewModel = StateObject(wrappedValue: viewModel())
    }

    var body: some View {
        LoadStateView(state: viewModel.state) { tracks in
            List {
                ForEach(tracks) { track in
                    TrackRow(track: track) {
                        environment.playback.play(tracks: tracks, startIndex: tracks.firstIndex(of: track) ?? 0)
                    }
                }
            }
        }
        .navigationTitle("Songs")
        .task { await viewModel.load() }
        .refreshable { await viewModel.load() }
    }
}

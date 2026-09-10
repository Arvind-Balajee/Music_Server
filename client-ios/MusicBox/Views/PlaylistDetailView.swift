import SwiftUI

struct PlaylistDetailView: View {
    @EnvironmentObject private var environment: AppEnvironment
    @StateObject private var viewModel: PlaylistDetailViewModel

    init(viewModel: @autoclosure @escaping () -> PlaylistDetailViewModel) {
        _viewModel = StateObject(wrappedValue: viewModel())
    }

    var body: some View {
        LoadStateView(state: viewModel.state) { detail in
            List {
                if detail.tracks.isEmpty {
                    Text("This playlist has no songs yet.")
                        .foregroundStyle(.secondary)
                } else {
                    ForEach(detail.tracks) { track in
                        TrackRow(track: track) {
                            environment.playback.play(tracks: detail.tracks, startIndex: detail.tracks.firstIndex(of: track) ?? 0)
                        }
                    }
                    .onDelete { offsets in
                        for index in offsets {
                            let track = detail.tracks[index]
                            Task { await viewModel.removeTrack(track.id) }
                        }
                    }
                }
            }
            .navigationTitle(detail.name)
        }
        .task { await viewModel.load() }
        .refreshable { await viewModel.load() }
    }
}

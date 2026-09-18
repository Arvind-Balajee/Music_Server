import SwiftUI

struct PlaylistDetailView: View {
    @EnvironmentObject private var environment: AppEnvironment
    @StateObject private var viewModel: PlaylistDetailViewModel
    @State private var showRenamePrompt = false
    @State private var renameText = ""

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
        .toolbar {
            ToolbarItem(placement: .primaryAction) { EditButton() }
            ToolbarItem(placement: .primaryAction) {
                Button {
                    renameText = viewModel.state.value?.name ?? ""
                    showRenamePrompt = true
                } label: {
                    Image(systemName: "pencil")
                }
            }
        }
        .alert("Rename Playlist", isPresented: $showRenamePrompt) {
            TextField("Name", text: $renameText)
            Button("Cancel", role: .cancel) {}
            Button("Save") {
                Task { await viewModel.rename(to: renameText) }
            }
        }
        .alert("Error", isPresented: Binding(
            get: { viewModel.errorBanner != nil },
            set: { if !$0 { viewModel.errorBanner = nil } }
        )) {
            Button("OK", role: .cancel) {}
        } message: {
            Text(viewModel.errorBanner ?? "")
        }
        .task { await viewModel.load() }
        .refreshable { await viewModel.load() }
    }
}

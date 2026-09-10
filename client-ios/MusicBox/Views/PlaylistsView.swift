import SwiftUI

struct PlaylistsView: View {
    @EnvironmentObject private var environment: AppEnvironment
    @StateObject private var viewModel: PlaylistsViewModel
    @State private var showingNewPlaylistPrompt = false
    @State private var newPlaylistName = ""

    init(viewModel: @autoclosure @escaping () -> PlaylistsViewModel) {
        _viewModel = StateObject(wrappedValue: viewModel())
    }

    var body: some View {
        LoadStateView(state: viewModel.state) { playlists in
            List {
                if playlists.isEmpty {
                    Text("No playlists yet. Tap + to create one.")
                        .foregroundStyle(.secondary)
                }
                ForEach(playlists) { playlist in
                    NavigationLink(value: playlist) {
                        PlaylistRow(playlist: playlist)
                    }
                }
                .onDelete { offsets in
                    for index in offsets {
                        let playlist = playlists[index]
                        Task { await viewModel.deletePlaylist(id: playlist.id) }
                    }
                }
            }
        }
        .navigationDestination(for: Playlist.self) { playlist in
            PlaylistDetailView(viewModel: PlaylistDetailViewModel(
                playlistId: playlist.id,
                repository: environment.playlistRepository
            ))
        }
        .navigationTitle("Playlists")
        .toolbar {
            ToolbarItem(placement: .primaryAction) {
                Button {
                    newPlaylistName = ""
                    showingNewPlaylistPrompt = true
                } label: {
                    Image(systemName: "plus")
                }
            }
        }
        .alert("New Playlist", isPresented: $showingNewPlaylistPrompt) {
            TextField("Name", text: $newPlaylistName)
            Button("Cancel", role: .cancel) {}
            Button("Create") {
                Task { await viewModel.createPlaylist(named: newPlaylistName) }
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

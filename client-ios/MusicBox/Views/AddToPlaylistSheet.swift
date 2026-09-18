import SwiftUI

/// "Add to Playlist" sheet, matching Apple Music's: pick an existing playlist
/// or create a new one, tap to add, checkmark shows what's already added.
/// Presented from `TrackRow`'s context menu.
struct AddToPlaylistSheet: View {
    @StateObject private var viewModel: AddToPlaylistViewModel
    @Environment(\.dismiss) private var dismiss
    @State private var showNewPlaylistPrompt = false
    @State private var newPlaylistName = ""

    init(viewModel: @autoclosure @escaping () -> AddToPlaylistViewModel) {
        _viewModel = StateObject(wrappedValue: viewModel())
    }

    var body: some View {
        NavigationStack {
            LoadStateView(state: viewModel.state) { playlists in
                if playlists.isEmpty {
                    ContentUnavailableView("No Playlists", systemImage: "music.note.list", description: Text("Tap + to create one."))
                } else {
                    List(playlists) { playlist in
                        let added = viewModel.addedPlaylistIds.contains(playlist.id)
                        Button {
                            Task { await viewModel.add(to: playlist) }
                        } label: {
                            HStack {
                                PlaylistRow(playlist: playlist)
                                Spacer()
                                if added {
                                    Image(systemName: "checkmark.circle.fill")
                                        .foregroundStyle(.green)
                                }
                            }
                        }
                        .buttonStyle(.plain)
                        .disabled(added)
                    }
                }
            }
            .navigationTitle("Add to Playlist")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .cancellationAction) {
                    Button("Done") { dismiss() }
                }
                ToolbarItem(placement: .primaryAction) {
                    Button {
                        newPlaylistName = ""
                        showNewPlaylistPrompt = true
                    } label: {
                        Image(systemName: "plus")
                    }
                }
            }
            .alert("New Playlist", isPresented: $showNewPlaylistPrompt) {
                TextField("Name", text: $newPlaylistName)
                Button("Cancel", role: .cancel) {}
                Button("Create") {
                    Task { await viewModel.createPlaylistAndAdd(named: newPlaylistName) }
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
        }
    }
}

import SwiftUI

struct HomeView: View {
    @EnvironmentObject private var environment: AppEnvironment
    @StateObject private var viewModel: HomeViewModel

    init(viewModel: @autoclosure @escaping () -> HomeViewModel) {
        _viewModel = StateObject(wrappedValue: viewModel())
    }

    var body: some View {
        List {
            Section("Server") {
                LoadStateView(state: viewModel.status) { status in
                    VStack(alignment: .leading, spacing: 4) {
                        Label("Connected", systemImage: "checkmark.circle.fill")
                            .foregroundStyle(.green)
                        Text("\(status.trackCount) tracks - \(status.libraryRoots) library root(s)")
                            .font(.caption)
                            .foregroundStyle(.secondary)
                        Text("Server version \(status.version)")
                            .font(.caption)
                            .foregroundStyle(.secondary)
                    }
                }
            }

            Section("Recently Added") {
                LoadStateView(state: viewModel.recentAlbums) { albums in
                    if albums.isEmpty {
                        Text("No albums yet.")
                            .foregroundStyle(.secondary)
                    } else {
                        ForEach(albums) { album in
                            NavigationLink(value: album) {
                                AlbumRow(album: album)
                            }
                        }
                    }
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
        .navigationTitle("MusicBox")
        .task { await viewModel.load() }
        .refreshable { await viewModel.load() }
    }
}

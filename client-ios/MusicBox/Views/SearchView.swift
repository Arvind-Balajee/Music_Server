import SwiftUI

struct SearchView: View {
    @EnvironmentObject private var environment: AppEnvironment
    @StateObject private var viewModel: SearchViewModel

    init(viewModel: @autoclosure @escaping () -> SearchViewModel) {
        _viewModel = StateObject(wrappedValue: viewModel())
    }

    var body: some View {
        content
            .navigationTitle("Search")
            .searchable(text: $viewModel.query, prompt: "Artists, albums, songs")
    }

    @ViewBuilder
    private var content: some View {
        switch viewModel.results {
        case .idle:
            ContentUnavailableView.search
        case .loading:
            ProgressView().frame(maxWidth: .infinity, maxHeight: .infinity)
        case .failed(let message):
            ContentUnavailableView("Search failed", systemImage: "exclamationmark.triangle", description: Text(message))
        case .loaded(let results):
            if results.artists.isEmpty && results.albums.isEmpty && results.tracks.isEmpty {
                ContentUnavailableView.search(text: viewModel.query)
            } else {
                List {
                    if !results.artists.isEmpty {
                        Section("Artists") {
                            ForEach(results.artists) { artist in
                                ArtistRow(artist: artist)
                            }
                        }
                    }
                    if !results.albums.isEmpty {
                        Section("Albums") {
                            ForEach(results.albums) { album in
                                AlbumRow(album: album)
                            }
                        }
                    }
                    if !results.tracks.isEmpty {
                        Section("Songs") {
                            ForEach(results.tracks) { track in
                                TrackRow(track: track) {
                                    environment.playback.play(tracks: results.tracks, startIndex: results.tracks.firstIndex(of: track) ?? 0)
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

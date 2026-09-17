import SwiftUI

/// Composition point for the tab bar. This is the one place that reads
/// `AppEnvironment` and hands each screen exactly the Repository/ViewModel it
/// needs via its initializer — child Views never touch `AppEnvironment` or
/// `MusicBoxAPIClient` directly, keeping the dependency flow one-directional
/// (Plan.md §11: Views -> ViewModels -> Repositories -> APIClient).
struct RootTabView: View {
    @EnvironmentObject private var environment: AppEnvironment
    @State private var showNowPlaying = false

    var body: some View {
        ZStack(alignment: .bottom) {
            TabView {
                NavigationStack {
                    HomeView(viewModel: HomeViewModel(
                        statusRepository: environment.statusRepository,
                        albumRepository: environment.albumRepository
                    ))
                }
                .tabItem { Label("Home", systemImage: "house") }

                NavigationStack {
                    ArtistsView(viewModel: ArtistsViewModel(repository: environment.artistRepository))
                }
                .tabItem { Label("Artists", systemImage: "music.mic") }

                NavigationStack {
                    AlbumsView(viewModel: AlbumsViewModel(repository: environment.albumRepository))
                }
                .tabItem { Label("Albums", systemImage: "square.stack") }

                NavigationStack {
                    SongsView(viewModel: SongsViewModel(repository: environment.trackRepository))
                }
                .tabItem { Label("Songs", systemImage: "music.note") }

                NavigationStack {
                    SearchView(viewModel: SearchViewModel(repository: environment.searchRepository))
                }
                .tabItem { Label("Search", systemImage: "magnifyingglass") }

                NavigationStack {
                    PlaylistsView(viewModel: PlaylistsViewModel(repository: environment.playlistRepository))
                }
                .tabItem { Label("Playlists", systemImage: "music.note.list") }

                NavigationStack {
                    SettingsView(
                        settings: environment.settings,
                        viewModel: SettingsViewModel(statusRepository: environment.statusRepository)
                    )
                }
                .tabItem { Label("Settings", systemImage: "gearshape") }
            }

            if environment.playback.currentTrack != nil {
                MiniPlayerBar(playback: environment.playback) {
                    showNowPlaying = true
                }
                .padding(.bottom, 49) // sit just above the tab bar
            }
        }
        .fullScreenCover(isPresented: $showNowPlaying) {
            NowPlayingView(viewModel: NowPlayingViewModel(playback: environment.playback))
        }
    }
}

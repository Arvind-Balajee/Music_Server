import SwiftUI

/// Vertical placement of the floating bars stacked above the tab bar.
private enum BottomBarMetrics {
    /// How far above the screen bottom the floating tab bar strip ends.
    static let tabBarInset: CGFloat = 49
    static let spacing: CGFloat = 8

    /// The mini player stacks above the search bar, which sits above the tab bar.
    static var miniPlayerInset: CGFloat {
        tabBarInset + InlineSearchBar.Metrics.height + spacing
    }
}

/// Composition point for the tab bar. This is the one place that reads
/// `AppEnvironment` and hands each screen exactly the Repository/ViewModel it
/// needs via its initializer — child Views never touch `AppEnvironment` or
/// `MusicBoxAPIClient` directly, keeping the dependency flow one-directional
/// (Plan.md §11: Views -> ViewModels -> Repositories -> APIClient).
struct RootTabView: View {
    @EnvironmentObject private var environment: AppEnvironment
    @State private var showNowPlaying = false
    @State private var isSearchActive = false

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

            if !isSearchActive, environment.playback.currentTrack != nil {
                MiniPlayerBar(playback: environment.playback) {
                    showNowPlaying = true
                }
                // MiniPlayerBar insets itself by 8, so this lands it on the
                // same 21pt edge inset as the search bar and tab bar.
                .padding(.horizontal, InlineSearchBar.Metrics.horizontalInset - 8)
                .padding(.bottom, BottomBarMetrics.miniPlayerInset)
            }

            InlineSearchBar(
                viewModel: SearchViewModel(repository: environment.searchRepository),
                isActive: $isSearchActive
            )
            .padding(.bottom, isSearchActive ? 0 : BottomBarMetrics.tabBarInset)
        }
        .fullScreenCover(isPresented: $showNowPlaying) {
            NowPlayingView(viewModel: NowPlayingViewModel(playback: environment.playback))
        }
    }
}

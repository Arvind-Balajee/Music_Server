import SwiftUI

/// Apple Music-style inline search. Collapsed, it's a floating capsule styled
/// to match the tab bar it sits above; tapped, it expands in place into a live
/// search field with results rendered inline above it — search never navigates
/// away to a separate screen, so there's no Search tab at all.
struct InlineSearchBar: View {
    @EnvironmentObject private var environment: AppEnvironment
    @StateObject private var viewModel: SearchViewModel
    @Binding var isActive: Bool
    @FocusState private var fieldFocused: Bool

    init(viewModel: @autoclosure @escaping () -> SearchViewModel, isActive: Binding<Bool>) {
        _viewModel = StateObject(wrappedValue: viewModel())
        _isActive = isActive
    }

    var body: some View {
        VStack(spacing: 0) {
            if isActive {
                results
                    .frame(maxWidth: .infinity, maxHeight: .infinity)
            }

            field
                .padding(.horizontal, isActive ? 12 : Metrics.horizontalInset)
                .padding(.bottom, isActive ? 6 : 0)
        }
        .background {
            // Full-bleed backdrop while searching, so the tab bar and the tab
            // content behind it are covered rather than peeking through.
            if isActive {
                Rectangle()
                    .fill(.regularMaterial)
                    .ignoresSafeArea()
            }
        }
    }

    // MARK: - Field

    @ViewBuilder
    private var field: some View {
        if isActive {
            HStack(spacing: 10) {
                Image(systemName: "magnifyingglass")
                    .foregroundStyle(.secondary)

                TextField("Artists, albums, songs", text: $viewModel.query)
                    .focused($fieldFocused)
                    .submitLabel(.search)
                    .autocorrectionDisabled()
                    .textInputAutocapitalization(.never)

                if !viewModel.query.isEmpty {
                    Button {
                        viewModel.query = ""
                    } label: {
                        Image(systemName: "xmark.circle.fill")
                            .foregroundStyle(.secondary)
                    }
                    .buttonStyle(.plain)
                }

                Button("Cancel") { deactivate() }
                    .buttonStyle(.plain)
                    .font(.subheadline)
                    .foregroundStyle(Color.accentColor)
            }
            .padding(.horizontal, 16)
            .frame(height: Metrics.height)
            // A distinctly-filled field, not `.regularMaterial`: the backdrop
            // behind it is already material, so a material field on top of it
            // reads as no field at all.
            .background(Color(.tertiarySystemFill), in: Capsule())
        } else {
            Button { activate() } label: {
                HStack(spacing: 10) {
                    Image(systemName: "magnifyingglass")
                    Text("Search")
                    Spacer()
                }
                .foregroundStyle(.secondary)
                .padding(.horizontal, 18)
                .frame(height: Metrics.height)
                // Matches the floating tab bar directly below it: the same
                // `.bar` material and soft elevation, so it reads as a second
                // floating bar rather than a flat patch of the page
                // (`.regularMaterial` here was within 2/255 of the page
                // background and effectively disappeared).
                .background(.bar, in: Capsule())
                .shadow(color: .black.opacity(0.12), radius: 8, y: 2)
            }
            .buttonStyle(.plain)
        }
    }

    // MARK: - Results

    @ViewBuilder
    private var results: some View {
        switch viewModel.results {
        case .idle:
            ContentUnavailableView(
                "Search Your Library",
                systemImage: "magnifyingglass",
                description: Text("Find artists, albums, and songs.")
            )
        case .loading:
            ProgressView().frame(maxWidth: .infinity, maxHeight: .infinity)
        case .failed(let message):
            ContentUnavailableView(
                "Search failed",
                systemImage: "exclamationmark.triangle",
                description: Text(message)
            )
        case .loaded(let found):
            if found.artists.isEmpty && found.albums.isEmpty && found.tracks.isEmpty {
                ContentUnavailableView.search(text: viewModel.query)
            } else {
                List {
                    if !found.artists.isEmpty {
                        Section("Artists") {
                            ForEach(found.artists) { ArtistRow(artist: $0) }
                        }
                    }
                    if !found.albums.isEmpty {
                        Section("Albums") {
                            ForEach(found.albums) { AlbumRow(album: $0) }
                        }
                    }
                    if !found.tracks.isEmpty {
                        Section("Songs") {
                            ForEach(found.tracks) { track in
                                TrackRow(track: track) {
                                    environment.playback.play(
                                        tracks: found.tracks,
                                        startIndex: found.tracks.firstIndex(of: track) ?? 0
                                    )
                                }
                            }
                        }
                    }
                }
                .listStyle(.plain)
                .scrollContentBackground(.hidden)
                .scrollDismissesKeyboard(.interactively)
            }
        }
    }

    // MARK: - Activation

    private func activate() {
        withAnimation(.spring(response: 0.32, dampingFraction: 0.86)) { isActive = true }
        fieldFocused = true
    }

    private func deactivate() {
        fieldFocused = false
        viewModel.query = ""
        withAnimation(.spring(response: 0.32, dampingFraction: 0.86)) { isActive = false }
    }

    enum Metrics {
        /// Sized and inset to sit with the floating tab bar pill below it,
        /// which is 62pt tall with a 21pt inset on each side.
        static let height: CGFloat = 56
        static let horizontalInset: CGFloat = 21
    }
}

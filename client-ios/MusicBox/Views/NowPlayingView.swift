import SwiftUI

/// Full-screen Now Playing player, styled after Apple Music: blurred artwork
/// backdrop, swipe-down-to-dismiss, a thin scrubber, big transport controls,
/// the system volume slider, and an AirPlay + Up Next row at the bottom.
struct NowPlayingView: View {
    @ObservedObject private var playback: PlaybackController
    private let viewModel: NowPlayingViewModel
    @Environment(\.dismiss) private var dismiss

    @State private var scrubProgress: Double?
    @State private var showQueue = false
    @GestureState private var dragOffset: CGFloat = 0

    init(viewModel: NowPlayingViewModel) {
        self.viewModel = viewModel
        self.playback = viewModel.playback
    }

    var body: some View {
        ZStack {
            background

            VStack(spacing: 0) {
                grabber
                topBar
                Spacer(minLength: 8)
                artwork
                Spacer(minLength: 28)
                titleArtist
                progressSection
                    .padding(.top, 20)
                controlsRow
                    .padding(.top, 28)
                volumeRow
                    .padding(.top, 24)
                bottomRow
                    .padding(.top, 20)
                Spacer(minLength: 8)
            }
            .padding(.horizontal, 28)
            .padding(.top, 8)
            .padding(.bottom, 12)
        }
        .foregroundStyle(.white)
        .offset(y: max(0, dragOffset))
        // `.simultaneousGesture` so this doesn't steal taps away from the
        // buttons/scrubber/sliders nested inside (see the same fix in
        // `MiniPlayerBar`).
        .simultaneousGesture(dismissGesture)
        .sheet(isPresented: $showQueue) {
            QueueView(playback: playback, viewModel: viewModel)
        }
    }

    // MARK: - Background

    private var background: some View {
        ZStack {
            if let track = viewModel.track, track.hasArtwork ?? false {
                AsyncImage(url: viewModel.artworkURL(for: track)) { phase in
                    if case .success(let image) = phase {
                        image.resizable().aspectRatio(contentMode: .fill)
                    } else {
                        fallbackGradient
                    }
                }
            } else {
                fallbackGradient
            }
            Color.black.opacity(0.55)
        }
        .blur(radius: 70)
        .ignoresSafeArea()
        .animation(.easeInOut(duration: 0.4), value: viewModel.track?.id)
    }

    private var fallbackGradient: some View {
        LinearGradient(
            colors: [Color(white: 0.25), Color(white: 0.08)],
            startPoint: .top, endPoint: .bottom
        )
    }

    // MARK: - Chrome

    private var grabber: some View {
        Capsule()
            .fill(Color.white.opacity(0.35))
            .frame(width: 36, height: 5)
            .padding(.bottom, 8)
    }

    private var topBar: some View {
        HStack {
            Button { dismiss() } label: {
                Image(systemName: "chevron.down")
                    .font(.system(size: 16, weight: .semibold))
                    .frame(width: 32, height: 32)
            }
            .buttonStyle(.plain)

            Spacer()

            VStack(spacing: 2) {
                Text("PLAYING FROM")
                    .font(.system(size: 10, weight: .semibold))
                    .foregroundStyle(.white.opacity(0.6))
                Text(viewModel.track?.album?.title ?? "Library")
                    .font(.system(size: 12, weight: .semibold))
                    .lineLimit(1)
            }

            Spacer()

            Color.clear.frame(width: 32, height: 32)
        }
    }

    // MARK: - Artwork

    private var artwork: some View {
        ArtworkView(
            url: viewModel.track.map { viewModel.artworkURL(for: $0) },
            hasArtwork: viewModel.track?.hasArtwork ?? false,
            cornerRadius: 10
        )
        .aspectRatio(1, contentMode: .fit)
        .frame(maxWidth: .infinity)
        .shadow(color: .black.opacity(0.4), radius: 24, y: 12)
        .scaleEffect(playback.isPlaying ? 1 : 0.94)
        .animation(.spring(response: 0.4, dampingFraction: 0.7), value: playback.isPlaying)
        .padding(.horizontal, 12)
    }

    // MARK: - Title / artist

    private var titleArtist: some View {
        VStack(spacing: 4) {
            Text(viewModel.track?.title ?? "Nothing Playing")
                .font(.title2.weight(.bold))
                .multilineTextAlignment(.center)
                .lineLimit(2)
            if let artist = viewModel.track?.artist.name {
                Text(artist)
                    .font(.body)
                    .foregroundStyle(.white.opacity(0.7))
                    .lineLimit(1)
            }
            if let error = viewModel.lastError {
                Text(error)
                    .font(.caption)
                    .foregroundStyle(.orange)
                    .multilineTextAlignment(.center)
                    .padding(.top, 4)
            }
        }
        .padding(.horizontal, 8)
    }

    // MARK: - Progress

    private var progressSection: some View {
        VStack(spacing: 6) {
            Scrubber(progress: scrubProgress ?? viewModel.progress) { fraction in
                scrubProgress = fraction
            } onEnded: { fraction in
                viewModel.seek(to: fraction * viewModel.duration)
                scrubProgress = nil
            }

            HStack {
                Text(viewModel.timeString(displayedTime))
                Spacer()
                Text("-" + viewModel.timeString(viewModel.duration - displayedTime))
            }
            .font(.caption.monospacedDigit())
            .foregroundStyle(.white.opacity(0.7))
        }
    }

    private var displayedTime: TimeInterval {
        guard let scrubProgress else { return viewModel.currentTime }
        return scrubProgress * viewModel.duration
    }

    // MARK: - Transport controls

    private var controlsRow: some View {
        HStack(spacing: 0) {
            Button { viewModel.toggleShuffle() } label: {
                Image(systemName: "shuffle")
                    .font(.system(size: 18))
                    .foregroundStyle(viewModel.shuffleEnabled ? Color.accentColor : .white.opacity(0.85))
                    .frame(maxWidth: .infinity)
            }
            Button { viewModel.skipToPrevious() } label: {
                Image(systemName: "backward.fill")
                    .font(.system(size: 30))
                    .frame(maxWidth: .infinity)
            }
            Button { viewModel.togglePlayPause() } label: {
                Image(systemName: viewModel.isPlaying ? "pause.fill" : "play.fill")
                    .font(.system(size: 46))
                    .frame(maxWidth: .infinity)
            }
            Button { viewModel.skipToNext() } label: {
                Image(systemName: "forward.fill")
                    .font(.system(size: 30))
                    .frame(maxWidth: .infinity)
            }
            Button { viewModel.cycleRepeatMode() } label: {
                Image(systemName: viewModel.repeatMode.systemImageName)
                    .font(.system(size: 18))
                    .foregroundStyle(viewModel.repeatMode == .off ? .white.opacity(0.85) : Color.accentColor)
                    .frame(maxWidth: .infinity)
            }
        }
        .buttonStyle(.plain)
    }

    // MARK: - Volume

    private var volumeRow: some View {
        HStack(spacing: 10) {
            Image(systemName: "speaker.fill")
                .font(.system(size: 11))
                .foregroundStyle(.white.opacity(0.6))
            SystemVolumeSlider()
                .frame(height: 24)
            Image(systemName: "speaker.wave.3.fill")
                .font(.system(size: 11))
                .foregroundStyle(.white.opacity(0.6))
        }
    }

    // MARK: - Bottom row

    private var bottomRow: some View {
        HStack {
            AirPlayButton()
                .frame(width: 28, height: 28)

            Spacer()

            Button { showQueue = true } label: {
                Image(systemName: "list.bullet")
                    .font(.system(size: 18))
                    .frame(width: 32, height: 32)
            }
            .buttonStyle(.plain)
        }
    }

    // MARK: - Dismiss gesture

    private var dismissGesture: some Gesture {
        DragGesture(minimumDistance: 12)
            .updating($dragOffset) { value, state, _ in
                guard value.translation.height > 0 else { return }
                state = value.translation.height
            }
            .onEnded { value in
                if value.translation.height > 120 || value.predictedEndTranslation.height > 400 {
                    dismiss()
                }
            }
    }
}

/// Thin capsule scrubber, matching Apple Music's Now Playing progress bar: no
/// persistent knob, the fill thickens slightly while actively dragging.
private struct Scrubber: View {
    let progress: Double
    let onChanged: (Double) -> Void
    let onEnded: (Double) -> Void

    @State private var isDragging = false

    var body: some View {
        GeometryReader { proxy in
            ZStack(alignment: .leading) {
                Capsule().fill(Color.white.opacity(0.25))
                Capsule().fill(Color.white)
                    .frame(width: max(6, proxy.size.width * min(max(progress, 0), 1)))
            }
            .frame(height: isDragging ? 8 : 4)
            .animation(.easeOut(duration: 0.12), value: isDragging)
            .frame(maxHeight: .infinity, alignment: .center)
            .contentShape(Rectangle())
            .gesture(
                DragGesture(minimumDistance: 0)
                    .onChanged { value in
                        isDragging = true
                        onChanged(min(max(value.location.x / proxy.size.width, 0), 1))
                    }
                    .onEnded { value in
                        isDragging = false
                        onEnded(min(max(value.location.x / proxy.size.width, 0), 1))
                    }
            )
        }
        .frame(height: 20)
    }
}

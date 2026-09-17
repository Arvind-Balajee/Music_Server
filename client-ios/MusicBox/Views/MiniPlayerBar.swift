import SwiftUI

/// Persistent bottom bar shown above the tab bar whenever something is loaded
/// into the player, matching Apple Music's mini-player: a hairline progress
/// indicator along the top edge, artwork, marquee-free title/artist, and
/// play/pause + next controls. Tap anywhere else to expand to full Now
/// Playing; swipe left/right to skip, matching Apple Music's gesture.
/// Presentation only — all state comes from `PlaybackController`.
struct MiniPlayerBar: View {
    @ObservedObject var playback: PlaybackController
    var onTap: () -> Void

    @State private var dragOffset: CGFloat = 0

    var body: some View {
        if let track = playback.currentTrack {
            VStack(spacing: 0) {
                ProgressBar(progress: progress)
                    .frame(height: 1.5)

                // NOT a `Button` wrapping the inner Play/Forward `Button`s: SwiftUI
                // doesn't reliably deliver taps to an outer `Button` when its label
                // contains other `Button`s (confirmed empirically -- XCUITest sees
                // the outer element as hittable, but its action never fires). A
                // plain tap gesture on a `.contentShape` region composes correctly
                // alongside real nested buttons instead.
                HStack(spacing: 12) {
                    ArtworkView(url: playback.artworkURL(for: track), hasArtwork: track.hasArtwork ?? false, cornerRadius: 5)
                        .frame(width: 40, height: 40)

                    VStack(alignment: .leading, spacing: 2) {
                        Text(track.title)
                            .font(.subheadline.weight(.semibold))
                            .lineLimit(1)
                        Text(track.artist.name)
                            .font(.caption)
                            .foregroundStyle(.secondary)
                            .lineLimit(1)
                    }

                    Spacer()

                    Button {
                        playback.togglePlayPause()
                    } label: {
                        Image(systemName: playback.isPlaying ? "pause.fill" : "play.fill")
                            .font(.title2)
                            .frame(width: 32, height: 32)
                    }
                    .buttonStyle(.plain)

                    Button {
                        playback.skipToNext()
                    } label: {
                        Image(systemName: "forward.fill")
                            .font(.title3)
                            .frame(width: 32, height: 32)
                    }
                    .buttonStyle(.plain)
                }
                .padding(.horizontal, 12)
                .padding(.vertical, 8)
                .contentShape(Rectangle())
                .onTapGesture(perform: onTap)
                .foregroundStyle(.primary)
            }
            .background(.regularMaterial, in: RoundedRectangle(cornerRadius: 12, style: .continuous))
            .clipShape(RoundedRectangle(cornerRadius: 12, style: .continuous))
            .padding(.horizontal, 8)
            .offset(x: dragOffset)
            // `.simultaneousGesture` (not `.gesture`) so this doesn't steal the
            // tap-to-expand hit-testing away from the `Button` it wraps -- a
            // plain `.gesture` here made the whole mini-player untappable.
            .simultaneousGesture(
                DragGesture(minimumDistance: 20)
                    .onChanged { value in
                        // Horizontal-only: a mostly-vertical drag is the user
                        // reaching for the tab bar or trying to expand, not a skip.
                        guard abs(value.translation.width) > abs(value.translation.height) else { return }
                        dragOffset = value.translation.width
                    }
                    .onEnded { value in
                        let horizontal = value.translation.width
                        let vertical = value.translation.height
                        defer {
                            withAnimation(.spring(response: 0.3, dampingFraction: 0.8)) { dragOffset = 0 }
                        }
                        guard abs(horizontal) > abs(vertical), abs(horizontal) > 60 else { return }
                        if horizontal < 0 {
                            playback.skipToNext()
                        } else {
                            playback.skipToPrevious()
                        }
                    }
            )
        }
    }

    private var progress: Double {
        guard playback.duration > 0 else { return 0 }
        return min(max(playback.currentTime / playback.duration, 0), 1)
    }
}

/// Hairline top-edge progress indicator, like the sliver above Apple Music's
/// mini-player.
private struct ProgressBar: View {
    let progress: Double

    var body: some View {
        GeometryReader { proxy in
            ZStack(alignment: .leading) {
                Color.secondary.opacity(0.2)
                Color.accentColor
                    .frame(width: proxy.size.width * progress)
            }
        }
    }
}

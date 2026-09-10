import SwiftUI

/// Persistent bottom bar shown above the tab bar whenever something is loaded
/// into the player, matching the common "tap to expand to full Now Playing"
/// pattern. Presentation only — all state comes from `PlaybackController`.
struct MiniPlayerBar: View {
    @ObservedObject var playback: PlaybackController
    var onTap: () -> Void

    var body: some View {
        if let track = playback.currentTrack {
            Button(action: onTap) {
                HStack(spacing: 12) {
                    RoundedRectangle(cornerRadius: 6)
                        .fill(Color.secondary.opacity(0.25))
                        .frame(width: 40, height: 40)
                        .overlay(Image(systemName: "music.note").foregroundStyle(.secondary))

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
                            .font(.title3)
                    }
                    .buttonStyle(.plain)

                    Button {
                        playback.skipToNext()
                    } label: {
                        Image(systemName: "forward.fill")
                            .font(.title3)
                    }
                    .buttonStyle(.plain)
                }
                .padding(.horizontal, 12)
                .padding(.vertical, 8)
                .background(.regularMaterial, in: RoundedRectangle(cornerRadius: 12))
                .padding(.horizontal, 8)
            }
            .buttonStyle(.plain)
            .foregroundStyle(.primary)
        }
    }
}

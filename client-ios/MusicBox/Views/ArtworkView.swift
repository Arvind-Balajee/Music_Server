import SwiftUI

/// Album/track artwork with a graceful fallback, used everywhere from list rows
/// up to the full-screen Now Playing artwork. Centralized here so every call
/// site (mini-player, Now Playing, queue rows) renders artwork/placeholder
/// identically instead of each hand-rolling its own `RoundedRectangle` stub.
struct ArtworkView: View {
    let url: URL?
    let hasArtwork: Bool
    var cornerRadius: CGFloat = 6

    var body: some View {
        RoundedRectangle(cornerRadius: cornerRadius, style: .continuous)
            .fill(placeholderGradient)
            .overlay {
                if hasArtwork, let url {
                    AsyncImage(url: url) { phase in
                        switch phase {
                        case .success(let image):
                            image.resizable().aspectRatio(contentMode: .fill)
                        case .empty:
                            ProgressView().controlSize(.small)
                        default:
                            placeholderIcon
                        }
                    }
                } else {
                    placeholderIcon
                }
            }
            .clipShape(RoundedRectangle(cornerRadius: cornerRadius, style: .continuous))
    }

    private var placeholderIcon: some View {
        Image(systemName: "music.note")
            .foregroundStyle(.white.opacity(0.85))
            .font(.system(size: 20, weight: .semibold))
    }

    private var placeholderGradient: LinearGradient {
        LinearGradient(
            colors: [Color.secondary.opacity(0.55), Color.secondary.opacity(0.25)],
            startPoint: .topLeading,
            endPoint: .bottomTrailing
        )
    }
}

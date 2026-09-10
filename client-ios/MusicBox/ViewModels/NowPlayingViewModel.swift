import Foundation

/// Thin presentation wrapper around the shared `PlaybackController`. Unlike the
/// other screens, Now Playing isn't backed by a Repository — its state comes
/// from live playback, not the network — so this ViewModel's job is purely
/// formatting (`timeString`, `progress`) and forwarding user intents to the
/// controller, keeping that logic out of `NowPlayingView`.
@MainActor
final class NowPlayingViewModel: ObservableObject {
    /// Exposed (not `private`) so `NowPlayingView` can `@ObservedObject` it
    /// directly for live UI updates, while still routing every user intent
    /// (play/pause/seek/...) through this ViewModel's methods below.
    let playback: PlaybackController

    init(playback: PlaybackController) {
        self.playback = playback
    }

    var track: Track? { playback.currentTrack }
    var isPlaying: Bool { playback.isPlaying }
    var currentTime: TimeInterval { playback.currentTime }
    var duration: TimeInterval { playback.duration }
    var shuffleEnabled: Bool { playback.shuffleEnabled }
    var repeatMode: RepeatMode { playback.repeatMode }
    var queue: [Track] { playback.queue }
    var lastError: String? { playback.lastError }

    var progress: Double {
        guard duration > 0 else { return 0 }
        return min(max(currentTime / duration, 0), 1)
    }

    func timeString(_ seconds: TimeInterval) -> String {
        guard seconds.isFinite, seconds >= 0 else { return "0:00" }
        let total = Int(seconds)
        return String(format: "%d:%02d", total / 60, total % 60)
    }

    func togglePlayPause() { playback.togglePlayPause() }
    func skipToNext() { playback.skipToNext() }
    func skipToPrevious() { playback.skipToPrevious() }
    func seek(to time: TimeInterval) { playback.seek(to: time) }
    func toggleShuffle() { playback.setShuffle(!playback.shuffleEnabled) }
    func cycleRepeatMode() { playback.cycleRepeatMode() }
}

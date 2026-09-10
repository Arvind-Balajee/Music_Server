import Foundation
import AVFoundation
import MediaPlayer
import Combine

/// Wraps `AVPlayer` with the queue/shuffle/repeat/lock-screen plumbing described
/// in `Plan.md` §11. This is a singleton-ish service owned by `AppEnvironment` and
/// shared across every screen that can trigger or observe playback (Songs,
/// Albums, Now Playing, mini-player, ...).
///
/// **Status: control-flow plumbing only.** The mock API's `streamUrl`s
/// (`/api/v1/tracks/{id}/stream`) don't resolve to real audio files yet — there is
/// no server to stream from during iOS-only development (Plan.md §25). `AVPlayer`
/// will be handed a real-looking-but-currently-404 URL, fail to load, and this
/// controller surfaces that as `lastError` without crashing. Every other piece —
/// play/pause/seek/next/previous/queue/shuffle/repeat, background audio session
/// setup, `MPNowPlayingInfoCenter`, `MPRemoteCommandCenter` — is real and has been
/// exercised by the unit tests in `MusicBoxTests`.
@MainActor
final class PlaybackController: ObservableObject {
    @Published private(set) var queue: [Track] = []
    @Published private(set) var currentIndex: Int?
    @Published private(set) var isPlaying: Bool = false
    @Published private(set) var currentTime: TimeInterval = 0
    @Published private(set) var duration: TimeInterval = 0
    @Published private(set) var shuffleEnabled: Bool = false
    @Published private(set) var repeatMode: RepeatMode = .off
    @Published private(set) var lastError: String?

    var currentTrack: Track? {
        guard let currentIndex, queue.indices.contains(currentIndex) else { return nil }
        return queue[currentIndex]
    }

    private let player: AVPlayer
    private let streamURLProvider: (TrackId) -> URL
    private var timeObserverToken: Any?
    private var shuffleOrder: [Int] = []
    private var itemEndObserver: NSObjectProtocol?

    /// - Parameter streamURLProvider: resolves a track id to its absolute stream
    ///   URL. Injected rather than depending on `MusicBoxAPIClient`/`TrackRepository`
    ///   directly so `PlaybackController` stays a leaf service usable from any
    ///   ViewModel without pulling in the whole networking stack.
    init(streamURLProvider: @escaping (TrackId) -> URL, player: AVPlayer = AVPlayer()) {
        self.streamURLProvider = streamURLProvider
        self.player = player
        configureAudioSession()
        configureRemoteCommandCenter()
        observePlayerTime()
    }

    deinit {
        if let timeObserverToken {
            player.removeTimeObserver(timeObserverToken)
        }
        if let itemEndObserver {
            NotificationCenter.default.removeObserver(itemEndObserver)
        }
    }

    // MARK: - Audio session / background playback

    private func configureAudioSession() {
        do {
            let session = AVAudioSession.sharedInstance()
            try session.setCategory(.playback, mode: .default, options: [])
            try session.setActive(true)
        } catch {
            lastError = "Audio session setup failed: \(error.localizedDescription)"
        }
    }

    // MARK: - Playing

    /// Replaces the queue and starts playing `tracks[startIndex]`.
    func play(tracks: [Track], startIndex: Int = 0) {
        guard tracks.indices.contains(startIndex) else { return }
        queue = tracks
        shuffleOrder = Array(tracks.indices)
        if shuffleEnabled { shuffleOrder.shuffle() }
        currentIndex = startIndex
        loadCurrentItem(autoplay: true)
    }

    /// Convenience for "play this one track now" from a list row.
    func play(track: Track) {
        play(tracks: [track], startIndex: 0)
    }

    func togglePlayPause() {
        isPlaying ? pause() : resume()
    }

    func resume() {
        guard currentTrack != nil else { return }
        player.play()
        isPlaying = true
        updateNowPlayingInfo()
    }

    func pause() {
        player.pause()
        isPlaying = false
        updateNowPlayingInfo()
    }

    func seek(to time: TimeInterval) {
        let cmTime = CMTime(seconds: time, preferredTimescale: 600)
        player.seek(to: cmTime, toleranceBefore: .zero, toleranceAfter: .zero)
        currentTime = time
        updateNowPlayingInfo()
    }

    func skipToNext() {
        advance(by: 1, wrapping: repeatMode == .all)
    }

    func skipToPrevious() {
        // Standard "restart current track if >3s in, else go back" behavior.
        if currentTime > 3 {
            seek(to: 0)
            return
        }
        advance(by: -1, wrapping: repeatMode == .all)
    }

    func setShuffle(_ enabled: Bool) {
        shuffleEnabled = enabled
        shuffleOrder = Array(queue.indices)
        if enabled { shuffleOrder.shuffle() }
    }

    func setRepeatMode(_ mode: RepeatMode) {
        repeatMode = mode
    }

    func cycleRepeatMode() {
        repeatMode = repeatMode.next
    }

    // MARK: - Internal playback mechanics

    private func advance(by delta: Int, wrapping: Bool) {
        guard let currentIndex, !queue.isEmpty else { return }
        let order = shuffleEnabled ? shuffleOrder : Array(queue.indices)
        guard let position = order.firstIndex(of: currentIndex) else { return }
        var nextPosition = position + delta

        if nextPosition < 0 {
            nextPosition = wrapping ? order.count - 1 : 0
        } else if nextPosition >= order.count {
            if wrapping {
                nextPosition = 0
            } else {
                pause()
                return
            }
        }

        self.currentIndex = order[nextPosition]
        loadCurrentItem(autoplay: isPlaying)
    }

    private func loadCurrentItem(autoplay: Bool) {
        guard let track = currentTrack else { return }
        let url = streamURLProvider(track.id)
        let item = AVPlayerItem(url: url)
        if let itemEndObserver {
            NotificationCenter.default.removeObserver(itemEndObserver)
        }
        itemEndObserver = NotificationCenter.default.addObserver(
            forName: .AVPlayerItemDidPlayToEndTime,
            object: item,
            queue: .main
        ) { [weak self] _ in
            // NotificationCenter's `queue: .main` guarantees this runs on the main
            // thread, but the compiler can't see that as satisfying @MainActor —
            // hop explicitly rather than silence the isolation check.
            Task { @MainActor [weak self] in
                self?.handlePlaybackEnded()
            }
        }

        player.replaceCurrentItem(with: item)
        duration = track.duration
        currentTime = 0
        lastError = nil

        if autoplay {
            player.play()
            isPlaying = true
        }
        updateNowPlayingInfo()
    }

    private func handlePlaybackEnded() {
        if repeatMode == .one {
            seek(to: 0)
            player.play()
            return
        }
        skipToNext()
    }

    private func observePlayerTime() {
        let interval = CMTime(seconds: 0.5, preferredTimescale: 600)
        timeObserverToken = player.addPeriodicTimeObserver(forInterval: interval, queue: .main) { [weak self] time in
            Task { @MainActor [weak self] in
                guard let self else { return }
                self.currentTime = time.seconds.isFinite ? time.seconds : 0
            }
        }
    }

    // MARK: - Lock screen / remote commands

    private func configureRemoteCommandCenter() {
        let center = MPRemoteCommandCenter.shared()

        center.playCommand.addTarget { [weak self] _ in
            self?.resume()
            return .success
        }
        center.pauseCommand.addTarget { [weak self] _ in
            self?.pause()
            return .success
        }
        center.togglePlayPauseCommand.addTarget { [weak self] _ in
            self?.togglePlayPause()
            return .success
        }
        center.nextTrackCommand.addTarget { [weak self] _ in
            self?.skipToNext()
            return .success
        }
        center.previousTrackCommand.addTarget { [weak self] _ in
            self?.skipToPrevious()
            return .success
        }
        center.changePlaybackPositionCommand.addTarget { [weak self] event in
            guard let event = event as? MPChangePlaybackPositionCommandEvent else { return .commandFailed }
            self?.seek(to: event.positionTime)
            return .success
        }
    }

    private func updateNowPlayingInfo() {
        guard let track = currentTrack else {
            MPNowPlayingInfoCenter.default().nowPlayingInfo = nil
            return
        }
        var info: [String: Any] = [
            MPMediaItemPropertyTitle: track.title,
            MPMediaItemPropertyArtist: track.artist.name,
            MPMediaItemPropertyPlaybackDuration: duration,
            MPNowPlayingInfoPropertyElapsedPlaybackTime: currentTime,
            MPNowPlayingInfoPropertyPlaybackRate: isPlaying ? 1.0 : 0.0,
        ]
        if let albumTitle = track.album?.title {
            info[MPMediaItemPropertyAlbumTitle] = albumTitle
        }
        MPNowPlayingInfoCenter.default().nowPlayingInfo = info
    }
}

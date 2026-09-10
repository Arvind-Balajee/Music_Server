import SwiftUI

struct NowPlayingView: View {
    @ObservedObject private var playback: PlaybackController
    private let viewModel: NowPlayingViewModel
    @State private var isScrubbing = false
    @State private var scrubTime: TimeInterval = 0

    init(viewModel: NowPlayingViewModel) {
        self.viewModel = viewModel
        self.playback = viewModel.playback
    }

    var body: some View {
        VStack(spacing: 24) {
            Spacer()

            RoundedRectangle(cornerRadius: 12)
                .fill(Color.secondary.opacity(0.2))
                .frame(width: 260, height: 260)
                .overlay(Image(systemName: "music.note").font(.system(size: 64)).foregroundStyle(.secondary))

            if let track = viewModel.track {
                VStack(spacing: 4) {
                    Text(track.title).font(.title2.weight(.semibold)).multilineTextAlignment(.center)
                    Text(track.artist.name).font(.body).foregroundStyle(.secondary)
                    if let album = track.album?.title {
                        Text(album).font(.caption).foregroundStyle(.secondary)
                    }
                }
                .padding(.horizontal)
            } else {
                Text("Nothing playing").foregroundStyle(.secondary)
            }

            if let error = viewModel.lastError {
                Text(error)
                    .font(.caption)
                    .foregroundStyle(.orange)
                    .multilineTextAlignment(.center)
                    .padding(.horizontal)
            }

            VStack(spacing: 6) {
                Slider(
                    value: Binding(
                        get: { isScrubbing ? scrubTime : viewModel.currentTime },
                        set: { scrubTime = $0 }
                    ),
                    in: 0...max(viewModel.duration, 1),
                    onEditingChanged: { editing in
                        isScrubbing = editing
                        if !editing { viewModel.seek(to: scrubTime) }
                    }
                )
                HStack {
                    Text(viewModel.timeString(isScrubbing ? scrubTime : viewModel.currentTime))
                    Spacer()
                    Text(viewModel.timeString(viewModel.duration))
                }
                .font(.caption)
                .foregroundStyle(.secondary)
            }
            .padding(.horizontal)

            HStack(spacing: 36) {
                Button { viewModel.toggleShuffle() } label: {
                    Image(systemName: "shuffle")
                        .foregroundStyle(viewModel.shuffleEnabled ? Color.accentColor : Color.secondary)
                }
                Button { viewModel.skipToPrevious() } label: {
                    Image(systemName: "backward.fill").font(.title2)
                }
                Button { viewModel.togglePlayPause() } label: {
                    Image(systemName: viewModel.isPlaying ? "pause.circle.fill" : "play.circle.fill")
                        .font(.system(size: 56))
                }
                Button { viewModel.skipToNext() } label: {
                    Image(systemName: "forward.fill").font(.title2)
                }
                Button { viewModel.cycleRepeatMode() } label: {
                    Image(systemName: viewModel.repeatMode.systemImageName)
                        .foregroundStyle(viewModel.repeatMode == .off ? Color.secondary : Color.accentColor)
                }
            }
            .buttonStyle(.plain)

            Spacer()

            if !viewModel.queue.isEmpty {
                VStack(alignment: .leading) {
                    Text("Up Next").font(.headline).padding(.horizontal)
                    List(viewModel.queue) { track in
                        TrackRow(track: track)
                    }
                    .listStyle(.plain)
                    .frame(maxHeight: 200)
                }
            }
        }
        .padding(.top)
        .navigationTitle("Now Playing")
        .navigationBarTitleDisplayMode(.inline)
    }
}

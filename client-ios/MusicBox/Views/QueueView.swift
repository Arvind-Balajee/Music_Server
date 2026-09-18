import SwiftUI

/// The "Up Next" sheet, mirroring Apple Music's queue: the currently-playing
/// track pinned at top, the rest of the queue below it, reorderable and
/// swipe-to-remove, tap-to-jump. Presented from `NowPlayingView`.
struct QueueView: View {
    @ObservedObject var playback: PlaybackController
    let viewModel: NowPlayingViewModel
    @Environment(\.dismiss) private var dismiss

    var body: some View {
        NavigationStack {
            List {
                if let current = viewModel.track, let currentIndex = playback.currentIndex {
                    Section("Now Playing") {
                        Button {
                            viewModel.playQueueItem(at: currentIndex)
                        } label: {
                            QueueRow(track: current, isCurrent: true, artworkURL: viewModel.artworkURL(for: current))
                        }
                        .buttonStyle(.plain)
                    }
                }

                let upNext = upNextEntries
                if !upNext.isEmpty {
                    Section("Up Next") {
                        ForEach(upNext, id: \.offset) { entry in
                            // A `Button`, not `.onTapGesture`: the list is
                            // permanently in edit mode (see `.environment`
                            // below) so that drag handles are always showing,
                            // and edit mode swallows plain row taps —
                            // buttons inside rows still fire.
                            Button {
                                viewModel.playQueueItem(at: entry.offset)
                            } label: {
                                QueueRow(track: entry.track, isCurrent: false, artworkURL: viewModel.artworkURL(for: entry.track))
                            }
                            .buttonStyle(.plain)
                            .swipeActions(edge: .trailing) {
                                Button(role: .destructive) {
                                    viewModel.removeQueueItems(atOffsets: IndexSet([entry.offset]))
                                } label: {
                                    Label("Remove", systemImage: "minus.circle")
                                }
                            }
                        }
                        .onMove { source, destination in
                            let mappedSource = IndexSet(source.map { upNext[$0].offset })
                            let mappedDestination = destination < upNext.count
                                ? upNext[destination].offset
                                : playback.queue.count
                            viewModel.moveQueueItems(fromOffsets: mappedSource, toOffset: mappedDestination)
                        }
                    }
                } else if viewModel.track != nil {
                    Section {
                        Text("No more songs in queue").foregroundStyle(.secondary)
                    }
                }
            }
            .listStyle(.insetGrouped)
            // Always editable, so every Up Next row carries a drag handle
            // without the user first having to find an Edit button — matching
            // Apple Music's queue. `.onMove` does nothing outside edit mode.
            .environment(\.editMode, .constant(.active))
            .navigationTitle("Up Next")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .cancellationAction) {
                    Button("Done") { dismiss() }
                }
            }
        }
    }

    /// Queue entries after the currently-playing track, each tagged with its
    /// absolute offset into `playback.queue` so moves/deletes/taps map back
    /// correctly even though this section is a slice of the full queue.
    private var upNextEntries: [(offset: Int, track: Track)] {
        guard let currentIndex = playback.currentIndex else {
            return Array(playback.queue.enumerated().map { ($0.offset, $0.element) })
        }
        return playback.queue.enumerated()
            .filter { $0.offset != currentIndex }
            .map { ($0.offset, $0.element) }
    }
}

private struct QueueRow: View {
    let track: Track
    let isCurrent: Bool
    let artworkURL: URL

    var body: some View {
        HStack(spacing: 12) {
            ArtworkView(url: artworkURL, hasArtwork: track.hasArtwork ?? false, cornerRadius: 4)
                .frame(width: 44, height: 44)

            VStack(alignment: .leading, spacing: 2) {
                Text(track.title)
                    .font(.subheadline.weight(isCurrent ? .semibold : .regular))
                    .foregroundStyle(isCurrent ? Color.accentColor : .primary)
                    .lineLimit(1)
                Text(track.artist.name)
                    .font(.caption)
                    .foregroundStyle(.secondary)
                    .lineLimit(1)
            }

            Spacer()

            if isCurrent {
                Image(systemName: "waveform")
                    .foregroundStyle(Color.accentColor)
            }
        }
        .contentShape(Rectangle())
    }
}

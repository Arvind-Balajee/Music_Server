import SwiftUI

struct AlbumRow: View {
    let album: Album

    var body: some View {
        HStack(spacing: 12) {
            RoundedRectangle(cornerRadius: 6)
                .fill(Color.secondary.opacity(0.2))
                .frame(width: 44, height: 44)
                .overlay(Image(systemName: "square.stack").foregroundStyle(.secondary))
            VStack(alignment: .leading, spacing: 2) {
                Text(album.title).font(.body)
                Text([album.artist?.name, album.year.map(String.init)].compactMap { $0 }.joined(separator: " - "))
                    .font(.caption)
                    .foregroundStyle(.secondary)
            }
        }
    }
}

struct ArtistRow: View {
    let artist: Artist

    var body: some View {
        HStack(spacing: 12) {
            Circle()
                .fill(Color.secondary.opacity(0.2))
                .frame(width: 44, height: 44)
                .overlay(Image(systemName: "music.mic").foregroundStyle(.secondary))
            Text(artist.name)
        }
    }
}

struct TrackRow: View {
    let track: Track
    var showAlbum: Bool = true
    var onPlay: (() -> Void)? = nil

    @EnvironmentObject private var environment: AppEnvironment
    @State private var showAddToPlaylist = false

    var body: some View {
        HStack(spacing: 12) {
            Button {
                onPlay?()
            } label: {
                Image(systemName: "play.circle")
                    .font(.title2)
                    .foregroundStyle(.secondary)
            }
            .buttonStyle(.plain)
            .disabled(onPlay == nil)

            VStack(alignment: .leading, spacing: 2) {
                Text(track.title)
                HStack(spacing: 4) {
                    Text(track.artist.name)
                    if showAlbum, let album = track.album?.title {
                        Text("-")
                        Text(album)
                    }
                }
                .font(.caption)
                .foregroundStyle(.secondary)
            }

            Spacer()

            Text(formattedDuration)
                .font(.caption)
                .foregroundStyle(.secondary)
        }
        .contentShape(Rectangle())
        .contextMenu {
            Button {
                environment.playback.playNext(track)
            } label: {
                Label("Play Next", systemImage: "text.insert")
            }
            Button {
                environment.playback.addToQueue(track)
            } label: {
                Label("Add to Queue", systemImage: "text.append")
            }
            Button {
                showAddToPlaylist = true
            } label: {
                Label("Add to Playlist", systemImage: "plus")
            }
        }
        .sheet(isPresented: $showAddToPlaylist) {
            AddToPlaylistSheet(viewModel: AddToPlaylistViewModel(track: track, repository: environment.playlistRepository))
        }
    }

    private var formattedDuration: String {
        let total = Int(track.duration)
        return String(format: "%d:%02d", total / 60, total % 60)
    }
}

struct PlaylistRow: View {
    let playlist: Playlist

    var body: some View {
        HStack(spacing: 12) {
            RoundedRectangle(cornerRadius: 6)
                .fill(Color.secondary.opacity(0.2))
                .frame(width: 44, height: 44)
                .overlay(Image(systemName: "music.note.list").foregroundStyle(.secondary))
            VStack(alignment: .leading, spacing: 2) {
                Text(playlist.name)
                if let count = playlist.trackCount {
                    Text("\(count) track\(count == 1 ? "" : "s")")
                        .font(.caption)
                        .foregroundStyle(.secondary)
                }
            }
        }
    }
}

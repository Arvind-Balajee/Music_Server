import Foundation

/// In-memory fixture implementation of `MusicBoxAPIClient`, per `Plan.md` §25
/// ("Mocks must be used to enable parallelism") — this is the app's default
/// implementation so every screen is runnable and demoable without a running
/// `musicbox-server`. The catalog below deliberately echoes the exact example
/// track from `Plan.md` §25 ("Test Song" / "Test Artist" / "Test Album") plus a
/// couple of real-looking albums (Daft Punk / Random Access Memories, Tame
/// Impala / Currents) drawn from the library-layout examples elsewhere in
/// `Plan.md`, so screenshots/demos look like a real library rather than
/// obviously-fake placeholder data.
///
/// Implemented as an `actor` because playlist CRUD needs mutable state that's
/// safe to call from multiple Tasks concurrently (e.g. a ViewModel racing a
/// prefetch). `baseURL` is `nonisolated` since it's an immutable, Sendable
/// stored property — see SE-0313 — so protocol conformance to the synchronous
/// `var baseURL: URL { get }` requirement doesn't force callers to `await` it.
actor MockMusicBoxAPIClient: MusicBoxAPIClient {
    nonisolated let baseURL: URL

    /// Simulated network latency so loading states in the UI are visible/testable
    /// even against the mock. Set to `.zero` in tests that want synchronous-feeling
    /// results.
    private let artificialDelay: Duration

    init(baseURL: URL = URL(string: "https://mock.musicbox.local")!, artificialDelay: Duration = .milliseconds(120)) {
        self.baseURL = baseURL
        self.artificialDelay = artificialDelay
    }

    // MARK: - Fixture catalog

    private static let daftPunk = Artist(id: 1, name: "Daft Punk", sortName: "Daft Punk")
    private static let tameImpala = Artist(id: 2, name: "Tame Impala", sortName: "Tame Impala")
    private static let testArtist = Artist(id: 3, name: "Test Artist")

    private static let allArtists: [Artist] = [daftPunk, tameImpala, testArtist]

    private static let randomAccessMemories = Album(
        id: 1, title: "Random Access Memories", artist: daftPunk.asRef, year: 2013, hasArtwork: true, trackCount: 3
    )
    private static let currents = Album(
        id: 2, title: "Currents", artist: tameImpala.asRef, year: 2015, hasArtwork: true, trackCount: 2
    )
    private static let testAlbum = Album(
        id: 3, title: "Test Album", artist: testArtist.asRef, hasArtwork: false, trackCount: 1
    )

    private static let allAlbums: [Album] = [randomAccessMemories, currents, testAlbum]

    private static let allTracks: [Track] = [
        Track(
            id: 1, title: "Give Life Back to Music",
            artist: daftPunk.asRef, album: randomAccessMemories.asRef,
            trackNumber: 1, discNumber: 1, durationMs: 274_000, codec: "flac",
            bitrateKbps: 1000, fileSize: 34_500_000, genre: "Electronic", hasArtwork: true,
            streamUrl: "/api/v1/tracks/1/stream"
        ),
        Track(
            id: 2, title: "Instant Crush",
            artist: daftPunk.asRef, album: randomAccessMemories.asRef,
            trackNumber: 5, discNumber: 1, durationMs: 337_000, codec: "flac",
            bitrateKbps: 1000, fileSize: 43_892_012, genre: "Electronic", hasArtwork: true,
            streamUrl: "/api/v1/tracks/2/stream"
        ),
        Track(
            id: 3, title: "Get Lucky",
            artist: daftPunk.asRef, album: randomAccessMemories.asRef,
            trackNumber: 8, discNumber: 1, durationMs: 369_000, codec: "mp3",
            bitrateKbps: 320, fileSize: 14_760_000, genre: "Funk", hasArtwork: true,
            streamUrl: "/api/v1/tracks/3/stream"
        ),
        Track(
            id: 4, title: "Let It Happen",
            artist: tameImpala.asRef, album: currents.asRef,
            trackNumber: 1, discNumber: 1, durationMs: 467_000, codec: "flac",
            bitrateKbps: 1000, fileSize: 58_400_000, genre: "Psychedelic Rock", hasArtwork: true,
            streamUrl: "/api/v1/tracks/4/stream"
        ),
        Track(
            id: 5, title: "The Less I Know the Better",
            artist: tameImpala.asRef, album: currents.asRef,
            trackNumber: 6, discNumber: 1, durationMs: 216_000, codec: "flac",
            bitrateKbps: 1000, fileSize: 27_000_000, genre: "Psychedelic Rock", hasArtwork: true,
            streamUrl: "/api/v1/tracks/5/stream"
        ),
        Track(
            id: 6, title: "Test Song",
            artist: testArtist.asRef, album: testAlbum.asRef,
            trackNumber: 1, durationMs: 210_000, codec: "mp3",
            streamUrl: "/api/v1/tracks/6/stream"
        ),
    ]

    /// Mutable playlist state (CRUD lives here, not in the static catalog above).
    private var playlistsById: [PlaylistId: PlaylistDetail] = [
        1: PlaylistDetail(
            id: 1, name: "Road Trip",
            createdAt: "2026-01-05T12:00:00Z", updatedAt: "2026-01-05T12:00:00Z",
            tracks: [allTracks[1], allTracks[3], allTracks[5]]
        ),
        2: PlaylistDetail(
            id: 2, name: "Favorites",
            createdAt: "2026-02-01T09:30:00Z", updatedAt: "2026-02-01T09:30:00Z",
            tracks: []
        ),
    ]
    private var nextPlaylistId: PlaylistId = 3

    private func simulateLatency() async {
        guard artificialDelay > .zero else { return }
        try? await Task.sleep(for: artificialDelay)
    }

    private func notFound(_ code: APIErrorCode, _ message: String) -> MusicBoxAPIError {
        .server(code: code.rawValue, message: message, httpStatus: 404)
    }

    // MARK: - Status

    func status() async throws -> StatusResponse {
        await simulateLatency()
        return StatusResponse(status: "ok", version: "0.1.0-mock", trackCount: Self.allTracks.count, libraryRoots: 1)
    }

    // MARK: - Artists

    func artists(limit: Int, offset: Int) async throws -> [Artist] {
        await simulateLatency()
        return Array(Self.allArtists.dropFirst(offset).prefix(limit))
    }

    func artist(id: ArtistId) async throws -> Artist {
        await simulateLatency()
        guard let artist = Self.allArtists.first(where: { $0.id == id }) else {
            throw notFound(.artistNotFound, "The requested artist does not exist.")
        }
        return artist
    }

    // MARK: - Albums

    func albums(artistId: ArtistId?, limit: Int, offset: Int) async throws -> [Album] {
        await simulateLatency()
        let filtered = Self.allAlbums.filter { artistId == nil || $0.artist?.id == artistId }
        return Array(filtered.dropFirst(offset).prefix(limit))
    }

    func album(id: AlbumId) async throws -> Album {
        await simulateLatency()
        guard let album = Self.allAlbums.first(where: { $0.id == id }) else {
            throw notFound(.albumNotFound, "The requested album does not exist.")
        }
        return album
    }

    // MARK: - Tracks

    func tracks(artistId: ArtistId?, albumId: AlbumId?, search: String?, limit: Int, offset: Int) async throws -> [Track] {
        await simulateLatency()
        let needle = search?.lowercased()
        let filtered = Self.allTracks.filter { track in
            if let artistId, track.artist.id != artistId { return false }
            if let albumId, track.album?.id != albumId { return false }
            if let needle, !needle.isEmpty {
                let haystack = [track.title, track.artist.name, track.album?.title ?? ""].joined(separator: " ").lowercased()
                return haystack.contains(needle)
            }
            return true
        }
        return Array(filtered.dropFirst(offset).prefix(limit))
    }

    func track(id: TrackId) async throws -> Track {
        await simulateLatency()
        guard let track = Self.allTracks.first(where: { $0.id == id }) else {
            throw notFound(.trackNotFound, "The requested track does not exist.")
        }
        return track
    }

    // MARK: - Search

    func search(query: String) async throws -> SearchResults {
        await simulateLatency()
        let needle = query.trimmingCharacters(in: .whitespacesAndNewlines).lowercased()
        guard !needle.isEmpty else { return .empty }

        let artists = Self.allArtists.filter { $0.name.lowercased().contains(needle) }
        let albums = Self.allAlbums.filter { $0.title.lowercased().contains(needle) }
        let tracks = Self.allTracks.filter { $0.title.lowercased().contains(needle) }
        return SearchResults(artists: artists, albums: albums, tracks: tracks)
    }

    // MARK: - Playlists

    func playlists() async throws -> [Playlist] {
        await simulateLatency()
        return playlistsById.values.map(\.summary).sorted { $0.id < $1.id }
    }

    func playlist(id: PlaylistId) async throws -> PlaylistDetail {
        await simulateLatency()
        guard let playlist = playlistsById[id] else {
            throw notFound(.playlistNotFound, "The requested playlist does not exist.")
        }
        return playlist
    }

    func createPlaylist(name: String) async throws -> PlaylistDetail {
        await simulateLatency()
        let id = nextPlaylistId
        nextPlaylistId += 1
        let now = ISO8601DateFormatter().string(from: Date())
        let playlist = PlaylistDetail(id: id, name: name, createdAt: now, updatedAt: now, tracks: [])
        playlistsById[id] = playlist
        return playlist
    }

    func updatePlaylist(id: PlaylistId, name: String) async throws -> PlaylistDetail {
        await simulateLatency()
        guard let existing = playlistsById[id] else {
            throw notFound(.playlistNotFound, "The requested playlist does not exist.")
        }
        let now = ISO8601DateFormatter().string(from: Date())
        let updated = PlaylistDetail(id: id, name: name, createdAt: existing.createdAt, updatedAt: now, tracks: existing.tracks)
        playlistsById[id] = updated
        return updated
    }

    func deletePlaylist(id: PlaylistId) async throws {
        await simulateLatency()
        guard playlistsById.removeValue(forKey: id) != nil else {
            throw notFound(.playlistNotFound, "The requested playlist does not exist.")
        }
    }

    func addTrack(_ trackId: TrackId, toPlaylist playlistId: PlaylistId) async throws {
        await simulateLatency()
        guard var playlist = playlistsById[playlistId] else {
            throw notFound(.playlistNotFound, "The requested playlist does not exist.")
        }
        guard let track = Self.allTracks.first(where: { $0.id == trackId }) else {
            throw notFound(.trackNotFound, "The requested track does not exist.")
        }
        guard !playlist.tracks.contains(where: { $0.id == trackId }) else { return }
        playlist = PlaylistDetail(
            id: playlist.id, name: playlist.name, createdAt: playlist.createdAt,
            updatedAt: ISO8601DateFormatter().string(from: Date()),
            tracks: playlist.tracks + [track]
        )
        playlistsById[playlistId] = playlist
    }

    func removeTrack(_ trackId: TrackId, fromPlaylist playlistId: PlaylistId) async throws {
        await simulateLatency()
        guard var playlist = playlistsById[playlistId] else {
            throw notFound(.playlistNotFound, "The requested playlist does not exist.")
        }
        playlist = PlaylistDetail(
            id: playlist.id, name: playlist.name, createdAt: playlist.createdAt,
            updatedAt: ISO8601DateFormatter().string(from: Date()),
            tracks: playlist.tracks.filter { $0.id != trackId }
        )
        playlistsById[playlistId] = playlist
    }
}

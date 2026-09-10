import XCTest
@testable import MusicBox

final class MockMusicBoxAPIClientTests: XCTestCase {
    private func makeClient() -> MockMusicBoxAPIClient {
        MockMusicBoxAPIClient(artificialDelay: .zero)
    }

    func testStatusReportsCatalogSize() async throws {
        let client = makeClient()
        let status = try await client.status()
        XCTAssertEqual(status.status, "ok")
        XCTAssertGreaterThan(status.trackCount, 0)
    }

    func testArtistsListAndGet() async throws {
        let client = makeClient()
        let artists = try await client.artists()
        XCTAssertFalse(artists.isEmpty)

        let first = try await client.artist(id: artists[0].id)
        XCTAssertEqual(first.id, artists[0].id)
    }

    func testArtistNotFoundThrowsTypedError() async throws {
        let client = makeClient()
        do {
            _ = try await client.artist(id: 9999)
            XCTFail("Expected artistNotFound error")
        } catch let error as MusicBoxAPIError {
            XCTAssertEqual(error.code, .artistNotFound)
        }
    }

    func testAlbumsFilteredByArtist() async throws {
        let client = makeClient()
        let artists = try await client.artists()
        guard let artist = artists.first else { return XCTFail("no fixture artists") }

        let albums = try await client.albums(byArtist: artist.id)
        XCTAssertTrue(albums.allSatisfy { $0.artist?.id == artist.id })
    }

    func testTracksFilteredByAlbumAndSearch() async throws {
        let client = makeClient()
        let albums = try await client.albums()
        guard let album = albums.first else { return XCTFail("no fixture albums") }

        let tracksInAlbum = try await client.tracks(byAlbum: album.id)
        XCTAssertTrue(tracksInAlbum.allSatisfy { $0.album?.id == album.id })

        let searchResults = try await client.tracks(artistId: nil, albumId: nil, search: "crush", limit: 100, offset: 0)
        XCTAssertTrue(searchResults.contains { $0.title.lowercased().contains("crush") })
    }

    func testSearchFansOutAcrossEntities() async throws {
        let client = makeClient()
        let results = try await client.search(query: "daft")
        XCTAssertFalse(results.artists.isEmpty)
        XCTAssertTrue(results.artists.allSatisfy { $0.name.lowercased().contains("daft") })
    }

    func testSearchWithBlankQueryReturnsEmpty() async throws {
        let client = makeClient()
        let results = try await client.search(query: "   ")
        XCTAssertEqual(results, .empty)
    }

    func testPlaylistCRUDLifecycle() async throws {
        let client = makeClient()

        let created = try await client.createPlaylist(name: "Road Songs")
        XCTAssertEqual(created.name, "Road Songs")
        XCTAssertTrue(created.tracks.isEmpty)

        let allTracks = try await client.tracks()
        guard let track = allTracks.first else { return XCTFail("no fixture tracks") }

        try await client.addTrack(track.id, toPlaylist: created.id)
        var reloaded = try await client.playlist(id: created.id)
        XCTAssertEqual(reloaded.tracks.map(\.id), [track.id])

        // Adding the same track twice should be idempotent, not duplicate it.
        try await client.addTrack(track.id, toPlaylist: created.id)
        reloaded = try await client.playlist(id: created.id)
        XCTAssertEqual(reloaded.tracks.count, 1)

        let renamed = try await client.updatePlaylist(id: created.id, name: "Road Trip Mix")
        XCTAssertEqual(renamed.name, "Road Trip Mix")

        try await client.removeTrack(track.id, fromPlaylist: created.id)
        reloaded = try await client.playlist(id: created.id)
        XCTAssertTrue(reloaded.tracks.isEmpty)

        try await client.deletePlaylist(id: created.id)
        do {
            _ = try await client.playlist(id: created.id)
            XCTFail("Expected playlistNotFound after deletion")
        } catch let error as MusicBoxAPIError {
            XCTAssertEqual(error.code, .playlistNotFound)
        }
    }

    func testAddTrackToMissingPlaylistThrows() async throws {
        let client = makeClient()
        do {
            try await client.addTrack(1, toPlaylist: 9999)
            XCTFail("Expected playlistNotFound error")
        } catch let error as MusicBoxAPIError {
            XCTAssertEqual(error.code, .playlistNotFound)
        }
    }

    func testStreamURLResolvesAgainstBaseURL() {
        let client = MockMusicBoxAPIClient(baseURL: URL(string: "https://example.com")!)
        XCTAssertEqual(client.streamURL(for: 42).absoluteString, "https://example.com/api/v1/tracks/42/stream")
    }
}

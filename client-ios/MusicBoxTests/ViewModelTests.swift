import XCTest
@testable import MusicBox

@MainActor
final class ViewModelTests: XCTestCase {
    private func makeMockClient() -> MockMusicBoxAPIClient {
        MockMusicBoxAPIClient(artificialDelay: .zero)
    }

    func testArtistsViewModelLoadsFromIdleToLoaded() async {
        let client = makeMockClient()
        let viewModel = ArtistsViewModel(repository: ArtistRepository(client: client))

        XCTAssertNil(viewModel.state.value)

        await viewModel.load()

        guard let artists = viewModel.state.value else {
            return XCTFail("expected loaded artists")
        }
        XCTAssertFalse(artists.isEmpty)
        XCTAssertNil(viewModel.state.errorMessage)
    }

    func testAlbumDetailViewModelSortsTracksByTrackNumber() async throws {
        let client = makeMockClient()
        let albums = try await client.albums()
        guard let album = albums.first else { return XCTFail("no fixture albums") }

        let viewModel = AlbumDetailViewModel(
            albumId: album.id,
            albumRepository: AlbumRepository(client: client),
            trackRepository: TrackRepository(client: client)
        )
        await viewModel.load()

        guard let tracks = viewModel.tracks.value, tracks.count > 1 else {
            return XCTFail("expected multiple tracks for fixture album")
        }
        let numbers = tracks.compactMap(\.trackNumber)
        XCTAssertEqual(numbers, numbers.sorted())
    }

    func testPlaylistsViewModelCreateAndDelete() async {
        let client = makeMockClient()
        let viewModel = PlaylistsViewModel(repository: PlaylistRepository(client: client))

        await viewModel.load()
        let initialCount = viewModel.state.value?.count ?? 0

        await viewModel.createPlaylist(named: "My New Mix")
        guard let afterCreate = viewModel.state.value else { return XCTFail("expected loaded state") }
        XCTAssertEqual(afterCreate.count, initialCount + 1)
        XCTAssertTrue(afterCreate.contains { $0.name == "My New Mix" })

        guard let created = afterCreate.first(where: { $0.name == "My New Mix" }) else {
            return XCTFail("could not find created playlist")
        }
        await viewModel.deletePlaylist(id: created.id)
        XCTAssertEqual(viewModel.state.value?.count, initialCount)
    }

    func testSearchViewModelDebouncesAndPopulatesResults() async throws {
        let client = makeMockClient()
        let viewModel = SearchViewModel(repository: SearchRepository(client: client), debounce: .milliseconds(10))

        viewModel.query = "tame"
        // Give the debounced Task time to fire.
        try await Task.sleep(for: .milliseconds(200))

        guard let results = viewModel.results.value else {
            return XCTFail("expected search results after debounce")
        }
        XCTAssertTrue(results.artists.contains { $0.name.lowercased().contains("tame") })
    }

    func testSearchViewModelClearingQueryResetsToIdle() async throws {
        let client = makeMockClient()
        let viewModel = SearchViewModel(repository: SearchRepository(client: client), debounce: .milliseconds(10))

        viewModel.query = "daft"
        try await Task.sleep(for: .milliseconds(200))
        XCTAssertNotNil(viewModel.results.value)

        viewModel.query = ""
        XCTAssertNil(viewModel.results.value)
    }
}

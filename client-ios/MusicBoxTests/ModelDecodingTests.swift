import XCTest
@testable import MusicBox

/// Decodes the exact JSON examples from `docs/api.md` and `Plan.md` §25 to catch
/// any drift between the client's `Codable` models and the documented server
/// contract before a real server exists to test against.
final class ModelDecodingTests: XCTestCase {
    func testDecodesTrackExampleFromApiDocs() throws {
        // Verbatim from docs/api.md `GET /api/v1/tracks/{id}`.
        let json = """
        {
            "id": 172,
            "title": "Instant Crush",
            "artist": { "id": 13, "name": "Daft Punk" },
            "album": { "id": 29, "title": "Random Access Memories" },
            "trackNumber": 5,
            "durationMs": 337000,
            "codec": "flac",
            "fileSize": 43892012,
            "streamUrl": "/api/v1/tracks/172/stream"
        }
        """
        let track = try JSONDecoder().decode(Track.self, from: Data(json.utf8))

        XCTAssertEqual(track.id, 172)
        XCTAssertEqual(track.title, "Instant Crush")
        XCTAssertEqual(track.artist.id, 13)
        XCTAssertEqual(track.artist.name, "Daft Punk")
        XCTAssertEqual(track.album?.id, 29)
        XCTAssertEqual(track.album?.title, "Random Access Memories")
        XCTAssertEqual(track.trackNumber, 5)
        XCTAssertEqual(track.durationMs, 337_000)
        XCTAssertEqual(track.codec, "flac")
        XCTAssertEqual(track.fileSize, 43_892_012)
        XCTAssertEqual(track.streamUrl, "/api/v1/tracks/172/stream")
        // Fields not present in the API example should decode to nil, not fail.
        XCTAssertNil(track.discNumber)
        XCTAssertNil(track.genre)
    }

    func testDecodesPlanMockTrackArrayExample() throws {
        // Verbatim from Plan.md §25 ("Mocks must be used to enable parallelism").
        let json = """
        [
            {
                "id": 1,
                "title": "Test Song",
                "artist": { "id": 1, "name": "Test Artist" },
                "album": { "id": 1, "title": "Test Album" },
                "durationMs": 210000,
                "streamUrl": "/api/v1/tracks/1/stream"
            }
        ]
        """
        let tracks = try JSONDecoder().decode([Track].self, from: Data(json.utf8))
        XCTAssertEqual(tracks.count, 1)
        XCTAssertEqual(tracks[0].title, "Test Song")
        XCTAssertEqual(tracks[0].artist.name, "Test Artist")
        // Plan.md's minimal example omits codec/trackNumber/fileSize entirely;
        // Track declares them optional precisely so this still decodes cleanly.
        XCTAssertNil(tracks[0].codec)
        XCTAssertNil(tracks[0].trackNumber)
        XCTAssertNil(tracks[0].fileSize)
    }

    func testDecodesStatusResponse() throws {
        let json = """
        { "status": "ok", "version": "0.1.0", "trackCount": 4213, "libraryRoots": 1 }
        """
        let status = try JSONDecoder().decode(StatusResponse.self, from: Data(json.utf8))
        XCTAssertEqual(status.status, "ok")
        XCTAssertEqual(status.trackCount, 4213)
        XCTAssertEqual(status.libraryRoots, 1)
    }

    func testDecodesErrorEnvelope() throws {
        let json = """
        { "error": { "code": "TRACK_NOT_FOUND", "message": "The requested track does not exist." } }
        """
        let envelope = try JSONDecoder().decode(APIErrorEnvelope.self, from: Data(json.utf8))
        XCTAssertEqual(envelope.error.code, "TRACK_NOT_FOUND")
        XCTAssertEqual(APIErrorCode(rawValue: envelope.error.code), .trackNotFound)
    }
}

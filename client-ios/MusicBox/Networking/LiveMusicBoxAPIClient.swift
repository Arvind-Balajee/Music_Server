import Foundation

/// Real `URLSession`-based conformance to `MusicBoxAPIClient`, talking to a
/// running `musicbox-server` per `docs/api.md`. Not yet exercised against a real
/// server (Agent 4's API is still in progress in parallel) — this implementation
/// is written directly against the documented contract and covered by decoding
/// unit tests against fixture JSON, but has not been integration-tested live.
final class LiveMusicBoxAPIClient: MusicBoxAPIClient, @unchecked Sendable {
    let baseURL: URL
    private let session: URLSession
    private let decoder: JSONDecoder
    private let encoder: JSONEncoder

    init(baseURL: URL, session: URLSession = .shared) {
        self.baseURL = baseURL
        self.session = session
        self.decoder = JSONDecoder()
        self.encoder = JSONEncoder()
    }

    // MARK: - Status

    func status() async throws -> StatusResponse {
        try await get("api/v1/status")
    }

    // MARK: - Artists

    func artists(limit: Int, offset: Int) async throws -> [Artist] {
        try await get("api/v1/artists", query: ["limit": "\(limit)", "offset": "\(offset)"])
    }

    func artist(id: ArtistId) async throws -> Artist {
        try await get("api/v1/artists/\(id)")
    }

    // MARK: - Albums

    func albums(artistId: ArtistId?, limit: Int, offset: Int) async throws -> [Album] {
        var query = ["limit": "\(limit)", "offset": "\(offset)"]
        if let artistId { query["artistId"] = "\(artistId)" }
        return try await get("api/v1/albums", query: query)
    }

    func album(id: AlbumId) async throws -> Album {
        try await get("api/v1/albums/\(id)")
    }

    // MARK: - Tracks

    func tracks(artistId: ArtistId?, albumId: AlbumId?, search: String?, limit: Int, offset: Int) async throws -> [Track] {
        var query = ["limit": "\(limit)", "offset": "\(offset)"]
        if let artistId { query["artistId"] = "\(artistId)" }
        if let albumId { query["albumId"] = "\(albumId)" }
        if let search { query["search"] = search }
        return try await get("api/v1/tracks", query: query)
    }

    func track(id: TrackId) async throws -> Track {
        try await get("api/v1/tracks/\(id)")
    }

    // MARK: - Search

    func search(query: String) async throws -> SearchResults {
        try await get("api/v1/search", query: ["q": query])
    }

    // MARK: - Playlists

    func playlists() async throws -> [Playlist] {
        try await get("api/v1/playlists")
    }

    func playlist(id: PlaylistId) async throws -> PlaylistDetail {
        try await get("api/v1/playlists/\(id)")
    }

    func createPlaylist(name: String) async throws -> PlaylistDetail {
        try await send("api/v1/playlists", method: "POST", body: PlaylistUpsertRequest(name: name))
    }

    func updatePlaylist(id: PlaylistId, name: String) async throws -> PlaylistDetail {
        try await send("api/v1/playlists/\(id)", method: "PUT", body: PlaylistUpsertRequest(name: name))
    }

    func deletePlaylist(id: PlaylistId) async throws {
        try await sendNoContent("api/v1/playlists/\(id)", method: "DELETE")
    }

    func addTrack(_ trackId: TrackId, toPlaylist playlistId: PlaylistId) async throws {
        try await sendNoContent(
            "api/v1/playlists/\(playlistId)/tracks",
            method: "POST",
            body: AddPlaylistTrackRequest(trackId: trackId)
        )
    }

    func removeTrack(_ trackId: TrackId, fromPlaylist playlistId: PlaylistId) async throws {
        try await sendNoContent("api/v1/playlists/\(playlistId)/tracks/\(trackId)", method: "DELETE")
    }

    // MARK: - Request plumbing

    private func makeURL(_ path: String, query: [String: String] = [:]) throws -> URL {
        guard var components = URLComponents(url: baseURL.appendingPathComponent(path), resolvingAgainstBaseURL: false) else {
            throw MusicBoxAPIError.invalidServerAddress
        }
        if !query.isEmpty {
            components.queryItems = query.map { URLQueryItem(name: $0.key, value: $0.value) }
        }
        guard let url = components.url else {
            throw MusicBoxAPIError.invalidServerAddress
        }
        return url
    }

    private func get<Response: Decodable>(_ path: String, query: [String: String] = [:]) async throws -> Response {
        let url = try makeURL(path, query: query)
        let request = URLRequest(url: url)
        return try await perform(request)
    }

    private func send<Body: Encodable, Response: Decodable>(
        _ path: String,
        method: String,
        body: Body
    ) async throws -> Response {
        var request = URLRequest(url: try makeURL(path))
        request.httpMethod = method
        request.setValue("application/json", forHTTPHeaderField: "Content-Type")
        request.httpBody = try encoder.encode(body)
        return try await perform(request)
    }

    private func sendNoContent(_ path: String, method: String) async throws {
        var request = URLRequest(url: try makeURL(path))
        request.httpMethod = method
        _ = try await performRaw(request)
    }

    private func sendNoContent<Body: Encodable>(_ path: String, method: String, body: Body) async throws {
        var request = URLRequest(url: try makeURL(path))
        request.httpMethod = method
        request.setValue("application/json", forHTTPHeaderField: "Content-Type")
        request.httpBody = try encoder.encode(body)
        _ = try await performRaw(request)
    }

    private func perform<Response: Decodable>(_ request: URLRequest) async throws -> Response {
        let data = try await performRaw(request)
        do {
            return try decoder.decode(Response.self, from: data)
        } catch {
            throw MusicBoxAPIError.decoding(String(describing: error))
        }
    }

    @discardableResult
    private func performRaw(_ request: URLRequest) async throws -> Data {
        let data: Data
        let response: URLResponse
        do {
            (data, response) = try await session.data(for: request)
        } catch {
            throw MusicBoxAPIError.transport(error.localizedDescription)
        }

        guard let http = response as? HTTPURLResponse else {
            throw MusicBoxAPIError.unexpectedStatus(-1)
        }

        guard (200..<300).contains(http.statusCode) else {
            if let envelope = try? decoder.decode(APIErrorEnvelope.self, from: data) {
                throw MusicBoxAPIError.server(
                    code: envelope.error.code,
                    message: envelope.error.message,
                    httpStatus: http.statusCode
                )
            }
            throw MusicBoxAPIError.unexpectedStatus(http.statusCode)
        }

        return data
    }
}

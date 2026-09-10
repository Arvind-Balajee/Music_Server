import Foundation

/// Matches the `Artist` shape in `docs/database.md` / `docs/api.md`.
struct Artist: Codable, Identifiable, Hashable, Sendable {
    let id: ArtistId
    let name: String
    let sortName: String?

    init(id: ArtistId, name: String, sortName: String? = nil) {
        self.id = id
        self.name = name
        self.sortName = sortName
    }
}

/// The compact `{ "id", "name" }` reference embedded inside a `Track` response
/// (see the `GET /api/v1/tracks/{id}` example in `docs/api.md`). Kept as a
/// separate type from `Artist` because the embedded form never carries `sortName`.
struct ArtistRef: Codable, Identifiable, Hashable, Sendable {
    let id: ArtistId
    let name: String
}

extension Artist {
    var asRef: ArtistRef { ArtistRef(id: id, name: name) }
}

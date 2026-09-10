import Foundation

// The server uses strong typed IDs internally (see docs/database.md: `Id<Tag>`),
// but over the wire every ID is a plain JSON integer. We keep type aliases so call
// sites read the way the server contract reads (`TrackId`, `ArtistId`, ...) without
// forcing a heavyweight wrapper type through Codable on the client.
typealias TrackId = Int
typealias ArtistId = Int
typealias AlbumId = Int
typealias PlaylistId = Int

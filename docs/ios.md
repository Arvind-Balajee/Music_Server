# iOS Application

Owner: Agent 5 (iOS Application). SwiftUI app under `client-ios/`, developed
against a mocked API (`Plan.md` §25) so it doesn't block on the C++ server.

## Status

Skeleton complete: all eight screens, the full `Views -> ViewModels ->
Repositories -> APIClient` layering, and AVFoundation playback plumbing are in
place and type-check cleanly against the iOS 17 simulator SDK. **Not yet run on
a simulator or device** — this development environment has no iOS Simulator
runtime installed (only the SDK headers ship with Xcode itself; the runtime
disk image is a separate multi-gigabyte download that wasn't available here).
See "Verification" below for exactly what was and wasn't checked.

## Project layout

```text
client-ios/
├── project.yml                  XcodeGen spec (iOS 17+, SwiftUI)
├── MusicBox.xcodeproj/          generated via `xcodegen generate` — do not hand-edit
├── MusicBox/
│   ├── App/                     MusicBoxApp entry point, AppEnvironment (DI root)
│   ├── Models/                  Codable structs matching docs/api.md exactly
│   ├── Networking/              MusicBoxAPIClient protocol + Live/Mock implementations
│   ├── Repositories/            thin seam between ViewModels and APIClient
│   ├── ViewModels/              one per screen, ObservableObject
│   ├── Views/                   SwiftUI views — no networking code here
│   ├── Playback/                PlaybackController (AVPlayer) + RepeatMode
│   └── Settings/                SettingsStore (UserDefaults-backed)
└── MusicBoxTests/                model decoding, mock client, and ViewModel tests
```

Regenerate the Xcode project after editing `project.yml` or adding/removing
files: `cd client-ios && xcodegen generate`.

## Architecture

`Views -> ViewModels -> Repositories -> APIClient -> MusicBox`, enforced, not
just a convention: no `URLSession`/`URLRequest` usage anywhere under `Views/`
or `ViewModels/` (verified by grep during integration review). Views observe
`ObservableObject` ViewModels; ViewModels call Repository methods; Repositories
call the `MusicBoxAPIClient` protocol; `AppEnvironment` is the single
composition root that decides whether `MockMusicBoxAPIClient` or
`LiveMusicBoxAPIClient` gets injected.

* **Models** (`Models/`): `Track`, `Album`, `Artist`, `Playlist`, `APIError` —
  `Codable` structs matching `docs/api.md`'s JSON shapes field-for-field.
* **`MusicBoxAPIClient`** (`Networking/MusicBoxAPIClient.swift`): protocol
  covering every endpoint in `docs/api.md` (status, artists/albums/tracks
  list+get, search, playlists CRUD + track add/remove) plus a
  `streamURL(for:)` helper.
  - `LiveMusicBoxAPIClient`: `URLSession`-based, takes a base URL.
  - `MockMusicBoxAPIClient`: an in-memory catalog shaped like `Plan.md` §25's
    example track, used as the default so the app is demoable without a
    running server.
* **Repositories** (`Repositories/`): one per resource type, hold an injected
  `MusicBoxAPIClient` — the seam where an on-device cache could be inserted
  later (not built yet, deliberately).
* **ViewModels**: `LoadState` (loading/loaded/error) drives each screen's
  `ObservableObject`.
* **Views**: Home, Artists (+ detail), Albums (+ detail), Songs, Search,
  Playlists (+ detail), Now Playing, Settings — per `Plan.md` §11.
* **Playback** (`Playback/PlaybackController.swift`): wraps `AVPlayer`;
  play/pause/seek/next/previous/queue/shuffle/repeat,
  `MPNowPlayingInfoCenter`/`MPRemoteCommandCenter` for lock-screen controls,
  background audio mode enabled in `project.yml`
  (`INFOPLIST_KEY_UIBackgroundModes: audio`). Streams from the mock's
  `streamUrl`s today — the control-flow plumbing is real, but there's no real
  audio behind it until Agent 2/4's server is reachable end-to-end.
* **Settings** (`Settings/SettingsStore.swift`): server address / mDNS name,
  persisted via `UserDefaults` — this is where the mock-vs-live `APIClient`
  switch lives.

No MusicKit, no DRM-related APIs — this only ever plays the user's own files
served by MusicBox.

## Networking specifics

`project.yml` sets `NSAppTransportSecurity` to allow insecure HTTP specifically
to the `musicbox.local` domain (and local network use generally), since the
server has no TLS (`docs/adr/0005-no-auth-in-mvp.md` — same "private LAN
device" posture applies to transport security). `NSLocalNetworkUsageDescription`
is set for the local-network permission prompt this requires on iOS 14+.

## Verification

**What was actually run and passed:**

```bash
swiftc -typecheck -sdk $(xcrun --sdk iphonesimulator --show-sdk-path) \
    -target arm64-apple-ios17.0-simulator $(find MusicBox -name '*.swift')
# exit 0, 0 errors, 0 warnings
```

Every file under `MusicBox/` type-checks cleanly against the real iOS 17
simulator SDK (two initial actor-isolation warnings in
`PlaybackController.swift`, from `NotificationCenter`/`AVPlayer` callbacks
running on `queue: .main` without the compiler being able to see that as
`@MainActor`-safe, were fixed by explicitly hopping with
`Task { @MainActor in ... }`). A grep audit confirmed no networking types
appear under `Views/` or `ViewModels/`.

**What could not be run in this environment, and why:**

```bash
xcodebuild -project MusicBox.xcodeproj -scheme MusicBox \
    -destination 'platform=iOS Simulator,name=Any iOS Simulator Device' build-for-testing
# error: no iOS Simulator runtime is installed (`xcrun simctl list runtimes` is empty);
# only the iphonesimulator26.2 SDK headers ship with Xcode itself.
```

This means the `MusicBoxTests` target (`ModelDecodingTests`,
`MockMusicBoxAPIClientTests`, `ViewModelTests`) has **not** been executed, and
the full app has never actually been built+linked+run in a simulator or on a
device — only individually type-checked. This is a real gap, not a formality:
run `xcodebuild test` on a machine with an iOS Simulator runtime installed (or
open `MusicBox.xcodeproj` in Xcode and hit Run) before treating this as
verified working software.

## Integration dependencies

Once Agent 4's real REST API is reachable, flip `AppEnvironment` to build
`LiveMusicBoxAPIClient` against the address in `SettingsStore` and confirm the
JSON models still decode the real server's responses (the `ModelDecodingTests`
target already asserts against `docs/api.md`'s documented shapes — extend it
with responses from a real running server once available). mDNS
discovery (`musicbox.local`, `docs/deployment.md`) is not yet wired into
`SettingsStore` — currently a manually-entered address/mDNS name field only.

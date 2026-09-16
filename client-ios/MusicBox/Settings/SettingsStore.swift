import Foundation
import Combine

/// Persisted app settings — server address / mDNS name and the mock-vs-live
/// toggle (Plan.md §11's Settings screen). Lives outside the View layer (see
/// `SettingsView`) so it stays a plain, testable model rather than scattered
/// `@AppStorage` calls; `AppEnvironment` observes it to decide which
/// `MusicBoxAPIClient` to build.
///
/// Per `Plan.md` §14, the preferred address is the mDNS hostname
/// (`musicbox.local`) with a fallback to a manually configured IP — this store
/// only persists the string the user provides; actual mDNS resolution vs. IP
/// fallback is not implemented yet (see docs/ios.md known limitations).
final class SettingsStore: ObservableObject {
    private enum Keys {
        static let serverHost = "musicbox.settings.serverHost"
        static let serverPort = "musicbox.settings.serverPort"
        static let useMockData = "musicbox.settings.useMockData"
        static let appearanceMode = "musicbox.settings.appearanceMode"
    }

    @Published var serverHost: String {
        didSet { defaults.set(serverHost, forKey: Keys.serverHost) }
    }

    @Published var serverPort: Int {
        didSet { defaults.set(serverPort, forKey: Keys.serverPort) }
    }

    /// When `true` (the default, so the app is demoable with zero setup), the app
    /// is wired to `MockMusicBoxAPIClient` regardless of `serverHost`/`serverPort`.
    @Published var useMockData: Bool {
        didSet { defaults.set(useMockData, forKey: Keys.useMockData) }
    }

    /// Light/Dark/System override — defaults to `.system` (follow the device),
    /// same "no setup required" philosophy as `useMockData` defaulting to `true`.
    @Published var appearanceMode: AppearanceMode {
        didSet { defaults.set(appearanceMode.rawValue, forKey: Keys.appearanceMode) }
    }

    private let defaults: UserDefaults

    init(defaults: UserDefaults = .standard) {
        self.defaults = defaults
        self.serverHost = defaults.string(forKey: Keys.serverHost) ?? "musicbox.local"
        self.serverPort = defaults.object(forKey: Keys.serverPort) as? Int ?? 8080
        self.useMockData = defaults.object(forKey: Keys.useMockData) as? Bool ?? true
        self.appearanceMode = defaults.string(forKey: Keys.appearanceMode)
            .flatMap(AppearanceMode.init(rawValue:)) ?? .system
    }

    /// `http://<host>:<port>` built from the persisted fields, or `nil` if the
    /// host is empty/unparseable.
    var resolvedBaseURL: URL? {
        let host = serverHost.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !host.isEmpty else { return nil }
        return URL(string: "http://\(host):\(serverPort)")
    }
}

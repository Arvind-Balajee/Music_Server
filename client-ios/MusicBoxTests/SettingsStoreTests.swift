import XCTest
@testable import MusicBox

final class SettingsStoreTests: XCTestCase {
    private func makeIsolatedDefaults() -> UserDefaults {
        let suiteName = "musicbox.tests.\(UUID().uuidString)"
        let defaults = UserDefaults(suiteName: suiteName)!
        addTeardownBlock { defaults.removePersistentDomain(forName: suiteName) }
        return defaults
    }

    func testAppearanceModeDefaultsToSystem() {
        let store = SettingsStore(defaults: makeIsolatedDefaults())
        XCTAssertEqual(store.appearanceMode, .system)
    }

    func testAppearanceModePersistsAcrossInstances() {
        let defaults = makeIsolatedDefaults()
        let first = SettingsStore(defaults: defaults)
        first.appearanceMode = .dark

        let second = SettingsStore(defaults: defaults)
        XCTAssertEqual(second.appearanceMode, .dark)
    }

    func testAppearanceModeColorSchemeMapping() {
        XCTAssertNil(AppearanceMode.system.colorScheme)
        XCTAssertEqual(AppearanceMode.light.colorScheme, .light)
        XCTAssertEqual(AppearanceMode.dark.colorScheme, .dark)
    }
}

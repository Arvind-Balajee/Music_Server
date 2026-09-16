import SwiftUI

@main
struct MusicBoxApp: App {
    // `settings` is held here too (not just inside `environment`) so this view
    // observes SettingsStore's own @Published changes directly -- AppEnvironment
    // doesn't forward its nested SettingsStore's objectWillChange, so reading
    // `environment.settings.appearanceMode` here wouldn't re-trigger `body` when
    // it changes. Both refer to the same SettingsStore instance (passed into
    // AppEnvironment's initializer below), so SettingsView's binding to
    // `environment.settings` still mutates the exact object this observes.
    @StateObject private var settings: SettingsStore
    @StateObject private var environment: AppEnvironment

    init() {
        let settings = SettingsStore()
        _settings = StateObject(wrappedValue: settings)
        _environment = StateObject(wrappedValue: AppEnvironment(settings: settings))
    }

    var body: some Scene {
        WindowGroup {
            RootTabView()
                .environmentObject(environment)
                .preferredColorScheme(settings.appearanceMode.colorScheme)
        }
    }
}

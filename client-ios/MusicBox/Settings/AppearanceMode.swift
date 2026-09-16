import SwiftUI

/// User-selectable appearance override for the app, independent of the
/// system's Light/Dark setting. Lives alongside `SettingsStore`, which
/// persists it.
enum AppearanceMode: String, CaseIterable, Identifiable, Sendable {
    case system
    case light
    case dark

    var id: String { rawValue }

    var label: String {
        switch self {
        case .system: return "System"
        case .light: return "Light"
        case .dark: return "Dark"
        }
    }

    /// `nil` for `.system` tells SwiftUI to defer to the device's current
    /// appearance rather than forcing one.
    var colorScheme: ColorScheme? {
        switch self {
        case .system: return nil
        case .light: return .light
        case .dark: return .dark
        }
    }
}

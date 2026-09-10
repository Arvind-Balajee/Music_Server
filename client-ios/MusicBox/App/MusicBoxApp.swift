import SwiftUI

@main
struct MusicBoxApp: App {
    @StateObject private var environment = AppEnvironment()

    var body: some Scene {
        WindowGroup {
            RootTabView()
                .environmentObject(environment)
        }
    }
}

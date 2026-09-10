import Foundation

@MainActor
final class SettingsViewModel: ObservableObject {
    @Published private(set) var connectionCheck: LoadState<StatusResponse> = .idle

    private let statusRepository: StatusRepository

    init(statusRepository: StatusRepository) {
        self.statusRepository = statusRepository
    }

    /// Re-created by the caller with a fresh `StatusRepository` after a settings
    /// change (see `SettingsView`), since `AppEnvironment.apiClient` may have
    /// swapped between mock and live.
    func testConnection() async {
        connectionCheck = .loading
        do {
            connectionCheck = .loaded(try await statusRepository.status())
        } catch {
            connectionCheck = .failed(error.localizedDescription)
        }
    }
}

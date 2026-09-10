import Foundation

/// Shared loading/error/data state used by every screen ViewModel, so Views can
/// switch over one shape (`ProgressView` / error banner / content) regardless of
/// which Repository backs the screen.
enum LoadState<Value> {
    case idle
    case loading
    case loaded(Value)
    case failed(String)

    var value: Value? {
        if case .loaded(let value) = self { return value }
        return nil
    }

    var isLoading: Bool {
        if case .loading = self { return true }
        return false
    }

    var errorMessage: String? {
        if case .failed(let message) = self { return message }
        return nil
    }
}

import SwiftUI

/// Renders the common idle/loading/error/content states so each screen doesn't
/// repeat the same `switch` boilerplate.
struct LoadStateView<Value, Content: View>: View {
    let state: LoadState<Value>
    @ViewBuilder var content: (Value) -> Content

    var body: some View {
        switch state {
        case .idle, .loading:
            ProgressView()
                .frame(maxWidth: .infinity, maxHeight: .infinity)
        case .failed(let message):
            ContentUnavailableView {
                Label("Something went wrong", systemImage: "exclamationmark.triangle")
            } description: {
                Text(message)
            }
        case .loaded(let value):
            content(value)
        }
    }
}

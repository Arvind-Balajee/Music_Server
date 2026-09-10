import Foundation

@MainActor
final class SearchViewModel: ObservableObject {
    @Published var query: String = "" {
        didSet { scheduleSearch() }
    }
    @Published private(set) var results: LoadState<SearchResults> = .idle

    private let repository: SearchRepository
    private var searchTask: Task<Void, Never>?

    /// Debounce delay before firing a search after the user stops typing.
    private let debounce: Duration

    init(repository: SearchRepository, debounce: Duration = .milliseconds(300)) {
        self.repository = repository
        self.debounce = debounce
    }

    private func scheduleSearch() {
        searchTask?.cancel()
        let trimmed = query.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !trimmed.isEmpty else {
            results = .idle
            return
        }

        searchTask = Task { [debounce] in
            try? await Task.sleep(for: debounce)
            guard !Task.isCancelled else { return }
            await self.performSearch(trimmed)
        }
    }

    private func performSearch(_ text: String) async {
        results = .loading
        do {
            let searchResults = try await repository.search(query: text)
            guard !Task.isCancelled else { return }
            results = .loaded(searchResults)
        } catch {
            guard !Task.isCancelled else { return }
            results = .failed(error.localizedDescription)
        }
    }
}

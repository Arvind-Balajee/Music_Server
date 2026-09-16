import SwiftUI

struct SettingsView: View {
    @ObservedObject private var settings: SettingsStore
    @StateObject private var viewModel: SettingsViewModel

    init(settings: SettingsStore, viewModel: @autoclosure @escaping () -> SettingsViewModel) {
        self.settings = settings
        _viewModel = StateObject(wrappedValue: viewModel())
    }

    var body: some View {
        Form {
            Section("Appearance") {
                Picker("Appearance", selection: $settings.appearanceMode) {
                    ForEach(AppearanceMode.allCases) { mode in
                        Text(mode.label).tag(mode)
                    }
                }
                .pickerStyle(.segmented)
            }

            Section {
                Toggle("Use Mock Data", isOn: $settings.useMockData)
            } footer: {
                Text("On by default so the app works without a MusicBox server. Turn this off once a real server is reachable at the address below.")
            }

            Section {
                HStack {
                    Text("Address")
                    Spacer()
                    TextField("musicbox.local", text: $settings.serverHost)
                        .multilineTextAlignment(.trailing)
                        .textInputAutocapitalization(.never)
                        .disableAutocorrection(true)
                        .disabled(settings.useMockData)
                }
                HStack {
                    Text("Port")
                    Spacer()
                    TextField("8080", value: $settings.serverPort, format: .number.grouping(.never))
                        .multilineTextAlignment(.trailing)
                        .keyboardType(.numberPad)
                        .disabled(settings.useMockData)
                }
            } header: {
                Text("MusicBox Server")
            } footer: {
                // Plan.md §14: prefer the mDNS hostname (musicbox.local); actual
                // mDNS resolution with IP fallback isn't implemented client-side
                // yet (see docs/ios.md known limitations) — this field is taken
                // as a literal hostname/IP for now.
                Text("Prefer the mDNS name (e.g. musicbox.local). Automatic mDNS discovery with IP fallback is not implemented yet.")
            }

            Section("Connection") {
                Button("Test Connection") {
                    Task { await viewModel.testConnection() }
                }
                .disabled(viewModel.connectionCheck.isLoading)

                switch viewModel.connectionCheck {
                case .idle:
                    EmptyView()
                case .loading:
                    ProgressView()
                case .failed(let message):
                    Label(message, systemImage: "xmark.octagon.fill")
                        .foregroundStyle(.red)
                        .font(.caption)
                case .loaded(let status):
                    Label(
                        "Connected - \(status.trackCount) tracks (v\(status.version))",
                        systemImage: "checkmark.circle.fill"
                    )
                    .foregroundStyle(.green)
                    .font(.caption)
                }
            }

            Section("About") {
                LabeledContent("App Version", value: "0.1.0")
                LabeledContent("Data Source", value: settings.useMockData ? "Mock" : "Live Server")
            }
        }
        .navigationTitle("Settings")
    }
}

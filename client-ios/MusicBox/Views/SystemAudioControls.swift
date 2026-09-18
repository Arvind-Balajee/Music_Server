import SwiftUI
import MediaPlayer
import AVKit
import AVFoundation

/// Reads and writes the device output volume.
///
/// Reading is plain public API — `AVAudioSession.outputVolume`, KVO-observed
/// so pressing the hardware volume buttons moves our UI too.
///
/// Writing has no public API at all: the only sanctioned way to change system
/// volume is a user drag on `MPVolumeView`'s own slider. So this keeps an
/// `MPVolumeView` mounted offscreen (writes only land while it's in a window)
/// and forwards new values to the `UISlider` inside it.
@MainActor
final class SystemVolumeController: ObservableObject {
    @Published private(set) var value: Double

    /// Mounted offscreen by `SystemVolumeSlider` — see the type doc.
    let volumeView: MPVolumeView = {
        let view = MPVolumeView(frame: CGRect(x: 0, y: 0, width: 200, height: 40))
        view.showsRouteButton = false
        return view
    }()

    private var observation: NSKeyValueObservation?

    init() {
        let session = AVAudioSession.sharedInstance()
        value = Double(session.outputVolume)
        observation = session.observe(\.outputVolume, options: [.new]) { [weak self] _, change in
            guard let newValue = change.newValue else { return }
            Task { @MainActor in self?.value = Double(newValue) }
        }
    }

    func set(_ newValue: Double) {
        let clamped = min(max(newValue, 0), 1)
        value = clamped
        embeddedSlider?.setValue(Float(clamped), animated: false)
    }

    /// `nil` in the Simulator: with no controllable output route there,
    /// `MPVolumeView` builds no slider at all (just a hidden placeholder
    /// label). Our own UI still renders and tracks the drag; there's simply no
    /// system volume behind it to move.
    private var embeddedSlider: UISlider? {
        volumeView.subviews.compactMap { $0 as? UISlider }.first
    }
}

/// The device volume slider on the Now Playing screen, drawn by us rather than
/// by `MPVolumeView` — `MPVolumeView` renders nothing at all in the Simulator,
/// and its stock blue `UISlider` looks out of place on the dark player anyway.
struct SystemVolumeSlider: View {
    @StateObject private var controller = SystemVolumeController()

    var body: some View {
        CapsuleSlider(
            value: controller.value,
            onChanged: { controller.set($0) },
            onEnded: { controller.set($0) }
        )
        .overlay {
            VolumeViewHost(volumeView: controller.volumeView)
                .frame(width: 1, height: 1)
                // Effectively invisible, but not `opacity(0)`/`hidden`, which
                // would stop it being mounted — and it only accepts volume
                // writes while it's live in the window.
                .opacity(0.001)
                .allowsHitTesting(false)
        }
    }
}

private struct VolumeViewHost: UIViewRepresentable {
    let volumeView: MPVolumeView

    func makeUIView(context: Context) -> MPVolumeView { volumeView }
    func updateUIView(_ uiView: MPVolumeView, context: Context) {}
}

/// AirPlay/output-device picker button, matching the one on Apple Music's Now
/// Playing screen.
struct AirPlayButton: UIViewRepresentable {
    func makeUIView(context: Context) -> AVRoutePickerView {
        let view = AVRoutePickerView(frame: .zero)
        view.prioritizesVideoDevices = false
        return view
    }

    func updateUIView(_ uiView: AVRoutePickerView, context: Context) {
        uiView.tintColor = .white
        uiView.activeTintColor = .white
    }
}

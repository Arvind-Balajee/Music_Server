import SwiftUI
import MediaPlayer
import AVKit

/// The device volume slider, matching the one Apple Music shows on its Now
/// Playing screen. Wraps `MPVolumeView` with its route button hidden — there's
/// no app-level "volume" concept to control (AVPlayer plays at the system
/// output volume), so this talks to the real system control directly rather
/// than faking a slider that wouldn't do anything.
struct SystemVolumeSlider: UIViewRepresentable {
    func makeUIView(context: Context) -> MPVolumeView {
        let view = MPVolumeView(frame: .zero)
        view.showsRouteButton = false
        view.showsVolumeSlider = true
        return view
    }

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

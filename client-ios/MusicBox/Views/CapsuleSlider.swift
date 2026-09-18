import SwiftUI

/// The thin capsule slider used on the Now Playing screen for both the
/// scrubber and the volume control, matching Apple Music: no persistent knob,
/// the fill just thickens while you're dragging it.
///
/// Colored for the dark Now Playing backdrop it's used against.
struct CapsuleSlider: View {
    /// Current position, 0...1.
    let value: Double
    let onChanged: (Double) -> Void
    let onEnded: (Double) -> Void

    @State private var isDragging = false

    var body: some View {
        GeometryReader { proxy in
            ZStack(alignment: .leading) {
                Capsule().fill(Color.white.opacity(0.25))
                Capsule().fill(Color.white)
                    .frame(width: max(6, proxy.size.width * min(max(value, 0), 1)))
            }
            .frame(height: isDragging ? 8 : 4)
            .animation(.easeOut(duration: 0.12), value: isDragging)
            .frame(maxHeight: .infinity, alignment: .center)
            .contentShape(Rectangle())
            .gesture(
                DragGesture(minimumDistance: 0)
                    .onChanged { drag in
                        isDragging = true
                        onChanged(fraction(of: drag.location.x, in: proxy.size.width))
                    }
                    .onEnded { drag in
                        isDragging = false
                        onEnded(fraction(of: drag.location.x, in: proxy.size.width))
                    }
            )
        }
        .frame(height: 20)
    }

    private func fraction(of x: CGFloat, in width: CGFloat) -> Double {
        guard width > 0 else { return 0 }
        return min(max(Double(x / width), 0), 1)
    }
}

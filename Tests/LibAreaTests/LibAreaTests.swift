import Testing
import LibArea

struct LibAreaTests {

    @Test func rectanglePocket() {
        let area = Area(accuracy: 0.01)
        // CCW rectangle 100 x 60
        area.addLineCurve([(0,0),(100,0),(100,60),(0,60),(0,0)])

        let curves = area.makePocket(toolRadius: 3.0, stepover: 2.5, mode: .spiral)
        #expect(!curves.isEmpty, "spiral pocket should produce at least one curve")
        #expect(curves.allSatisfy { !$0.vertices.isEmpty })
    }

    @Test func rectangleWithIslandPocket() {
        let area = Area(accuracy: 0.01)
        // CCW boundary
        area.addLineCurve([(0,0),(100,0),(100,100),(0,100),(0,0)])
        // CW island (reversed = hole)
        area.addLineCurve([(35,35),(35,65),(65,65),(65,35),(35,35)])

        let curves = area.makePocket(toolRadius: 3.0, stepover: 2.5, mode: .spiral)
        #expect(!curves.isEmpty)
    }

    @Test func zigZagPocket() {
        let area = Area(accuracy: 0.01)
        area.addLineCurve([(0,0),(100,0),(100,60),(0,60),(0,0)])

        let curves = area.makePocket(toolRadius: 3.0, stepover: 2.5, mode: .zigZag)
        #expect(!curves.isEmpty)
    }

    @Test func vertexStructure() {
        let area = Area(accuracy: 0.01)
        area.addLineCurve([(0,0),(50,0),(50,50),(0,50),(0,0)])

        let curves = area.makePocket(toolRadius: 3.0, stepover: 2.5, mode: .spiral)
        guard let first = curves.first else {
            Issue.record("No curves generated")
            return
        }
        // First vertex is the start point (line type)
        guard let start = first.startPoint else {
            Issue.record("Curve has no vertices")
            return
        }
        #expect(first.vertices[0].segmentType == .line,
                "First vertex should be a line (start point)")
        // Start point should be inside or near the boundary
        #expect(start.x >= -5 && start.x <= 55)
        #expect(start.y >= -5 && start.y <= 55)
    }
}

// Equatable conformance for test comparisons
extension PocketVertex.SegmentType: Equatable {
    public static func == (lhs: Self, rhs: Self) -> Bool {
        switch (lhs, rhs) {
        case (.line, .line): return true
        case (.ccwArc, .ccwArc): return true
        case (.cwArc, .cwArc): return true
        default: return false
        }
    }
}

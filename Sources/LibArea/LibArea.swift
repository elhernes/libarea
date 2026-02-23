import CLibArea
import Foundation

// ---------------------------------------------------------------------------
// MARK: - Point2D
// ---------------------------------------------------------------------------

public struct Point2D: Sendable {
    public var x, y: Double
    public init(_ x: Double, _ y: Double) { self.x = x; self.y = y }

    public static let zero = Point2D(0, 0)

    public static func - (a: Point2D, b: Point2D) -> Point2D { Point2D(a.x - b.x, a.y - b.y) }
    public static func + (a: Point2D, b: Point2D) -> Point2D { Point2D(a.x + b.x, a.y + b.y) }

    public var length: Double { (x*x + y*y).squareRoot() }
}

// ---------------------------------------------------------------------------
// MARK: - InputCurve
// ---------------------------------------------------------------------------

/// A 2-D closed input curve suitable for use as an area boundary or island.
///
/// Vertices follow the libarea convention:
/// - `vertices[0]` — start point (type == 0, arc fields unused).
/// - `vertices[1...]` — path segments; each vertex records the segment's end
///   point, its arc type, and the arc centre (for arc segments).
///
/// CCW winding = boundary; CW winding = island.
public struct InputCurve: Sendable {

    // MARK: Vertex

    public struct Vertex: Sendable {
        public let x, y: Double       // end point (or start point for vertex 0)
        public let cx, cy: Double     // arc centre (type != 0 only)
        /// 0 = line / start point, 1 = CCW arc, -1 = CW arc
        public let type: Int

        public static func line(_ x: Double, _ y: Double) -> Vertex {
            Vertex(x: x, y: y, cx: 0, cy: 0, type: 0)
        }
        public static func ccwArc(to x: Double, _ y: Double, center cx: Double, _ cy: Double) -> Vertex {
            Vertex(x: x, y: y, cx: cx, cy: cy, type: 1)
        }
        public static func cwArc(to x: Double, _ y: Double, center cx: Double, _ cy: Double) -> Vertex {
            Vertex(x: x, y: y, cx: cx, cy: cy, type: -1)
        }
    }

    public let vertices: [Vertex]

    public init(vertices: [Vertex]) { self.vertices = vertices }

    // MARK: Factory methods

    /// CCW rectangle (open — start point NOT repeated at end; CLibArea closes implicitly).
    public static func rect(x: Double, y: Double, width: Double, height: Double) -> InputCurve {
        InputCurve(vertices: [
            .line(x,         y),
            .line(x + width, y),
            .line(x + width, y + height),
            .line(x,         y + height),
            .line(x,         y),           // explicit close
        ])
    }

    /// CCW square.
    public static func square(x: Double, y: Double, size: Double) -> InputCurve {
        rect(x: x, y: y, width: size, height: size)
    }

    /// CCW circle encoded as four quarter-arcs.
    public static func circle(cx: Double, cy: Double, radius r: Double) -> InputCurve {
        InputCurve(vertices: [
            .line(cx + r, cy),
            .ccwArc(to: cx,     cy + r, center: cx, cy),
            .ccwArc(to: cx - r, cy,     center: cx, cy),
            .ccwArc(to: cx,     cy - r, center: cx, cy),
            .ccwArc(to: cx + r, cy,     center: cx, cy),
        ])
    }

    // MARK: Operations

    /// Reverse the winding (CCW ↔ CW) and flip arc directions.
    ///
    /// For a curve with vertices [v0, v1, …, vN] (vN.pos == v0.pos), the reversed
    /// curve starts at the same position as v0 and visits positions in reverse order,
    /// with arc directions flipped.
    public func reversed() -> InputCurve {
        guard vertices.count >= 2 else { return self }
        let N = vertices.count - 1   // number of segments

        var result: [Vertex] = []
        result.reserveCapacity(vertices.count)

        // New start = same position as v0 (since the closed curve ends where it starts)
        result.append(.line(vertices[0].x, vertices[0].y))

        // For k in 1...N: the k-th reversed segment ends at vertices[N-k]
        // and uses the flipped type of vertices[N-k+1] with the same centre.
        for k in 1...N {
            let seg = vertices[N - k + 1]   // original segment whose direction we flip
            let pt  = vertices[N - k]        // endpoint of the reversed segment
            let flippedType: Int
            switch seg.type {
            case 1:  flippedType = -1
            case -1: flippedType =  1
            default: flippedType =  0
            }
            result.append(Vertex(x: pt.x, y: pt.y, cx: seg.cx, cy: seg.cy, type: flippedType))
        }
        return InputCurve(vertices: result)
    }

    /// Replace every arc segment with a chain of short line approximations.
    /// The result has only type-0 vertices and is safe for ray-casting PIP tests.
    public func linearized(accuracy: Double = 0.1) -> InputCurve {
        guard !vertices.isEmpty else { return self }

        var result: [Vertex] = []
        var curX = vertices[0].x
        var curY = vertices[0].y
        result.append(.line(curX, curY))

        for v in vertices.dropFirst() {
            if v.type == 0 {
                result.append(.line(v.x, v.y))
            } else {
                let isCCW = (v.type == 1)
                let (centX, centY) = (v.cx, v.cy)
                let r = ((curX - centX)*(curX - centX) + (curY - centY)*(curY - centY)).squareRoot()
                let a0 = atan2(curY - centY, curX - centX)
                let a1 = atan2(v.y  - centY, v.x  - centX)
                var sweep = isCCW ? (a1 - a0) : (a0 - a1)
                if sweep <= 0 { sweep += 2 * .pi }
                let arcLen = r * sweep
                let n = max(1, Int(ceil(arcLen / accuracy)))
                let dA = (isCCW ? 1.0 : -1.0) * sweep / Double(n)
                for i in 1...n {
                    let a = a0 + Double(i) * dA
                    result.append(.line(centX + r * cos(a), centY + r * sin(a)))
                }
            }
            curX = v.x; curY = v.y
        }
        return InputCurve(vertices: result)
    }

    /// Axis-aligned bounding box computed from vertex positions only.
    /// For arc curves use `linearized().boundingBox` for tighter bounds.
    public var boundingBox: (minX: Double, minY: Double, maxX: Double, maxY: Double) {
        var mnX = Double.infinity, mnY = Double.infinity
        var mxX = -Double.infinity, mxY = -Double.infinity
        for v in vertices {
            if v.x < mnX { mnX = v.x }; if v.x > mxX { mxX = v.x }
            if v.y < mnY { mnY = v.y }; if v.y > mxY { mxY = v.y }
        }
        return (mnX, mnY, mxX, mxY)
    }

    /// Signed area (shoelace formula on vertices).  Positive = CCW in standard Y-up coords.
    public var signedArea: Double {
        var s = 0.0
        for i in 0..<vertices.count {
            let j = (i + 1) % vertices.count
            s += vertices[i].x * vertices[j].y - vertices[j].x * vertices[i].y
        }
        return s * 0.5
    }

    public var isCCW: Bool { signedArea > 0 }
}

// ---------------------------------------------------------------------------
// MARK: - PocketMode
// ---------------------------------------------------------------------------

public enum PocketMode: Sendable {
    case spiral
    case zigZag
    case singleOffset
    case zigZagThenSingleOffset

    var cValue: AreaPocketMode {
        switch self {
        case .spiral:                   return AREA_POCKET_SPIRAL
        case .zigZag:                   return AREA_POCKET_ZIGZAG
        case .singleOffset:             return AREA_POCKET_SINGLE_OFFSET
        case .zigZagThenSingleOffset:   return AREA_POCKET_ZIGZAG_THEN_SINGLE_OFFSET
        }
    }
}

// ---------------------------------------------------------------------------
// MARK: - PocketVertex / PocketCurve
// ---------------------------------------------------------------------------

public struct PocketVertex: Sendable {
    public enum SegmentType: Sendable {
        case line
        case ccwArc(center: Point2D)
        case cwArc(center: Point2D)
    }
    public let endPoint: Point2D
    public let segmentType: SegmentType

    init(_ raw: AreaVertexInfo) {
        endPoint = Point2D(raw.x, raw.y)
        switch raw.type {
        case 1:  segmentType = .ccwArc(center: Point2D(raw.cx, raw.cy))
        case -1: segmentType = .cwArc(center:  Point2D(raw.cx, raw.cy))
        default: segmentType = .line
        }
    }
}

public struct PocketCurve: Sendable {
    public let vertices: [PocketVertex]
    public var startPoint: Point2D? { vertices.first?.endPoint }
}

// ---------------------------------------------------------------------------
// MARK: - Area
// ---------------------------------------------------------------------------

public final class Area: Sendable {

    private let ref: AreaRef

    public init(accuracy: Double = 0.01) {
        ref = area_create(accuracy)
    }

    deinit { area_free(ref) }

    // MARK: Adding geometry

    /// Add a curve (boundary = CCW, island = CW) from an `InputCurve`.
    public func addCurve(_ curve: InputCurve) {
        area_begin_curve(ref)
        for v in curve.vertices {
            if v.type == 0 {
                area_add_line_vertex(ref, v.x, v.y)
            } else {
                area_add_arc_vertex(ref, v.x, v.y, v.cx, v.cy, v.type == 1 ? 1 : 0)
            }
        }
        area_end_curve(ref)
    }

    /// Add an island (reverses the curve to CW before adding).
    public func addIsland(_ curve: InputCurve) {
        addCurve(curve.reversed())
    }

    /// Add a line-only boundary from coordinate pairs.
    public func addLineCurve(_ points: [(x: Double, y: Double)]) {
        area_begin_curve(ref)
        for p in points { area_add_line_vertex(ref, p.x, p.y) }
        area_end_curve(ref)
    }

    // MARK: Boolean operations

    /// Compute the union of this area and `other` (modifies this area in place).
    public func unite(with other: Area) {
        area_unite(ref, other.ref)
    }

    /// Subtract `other` from this area (modifies this area in place).
    public func subtract(_ other: Area) {
        area_subtract(ref, other.ref)
    }

    // MARK: Reading back stored curves

    /// All curves currently stored in the area (boundary + islands).
    /// After a boolean operation these reflect the result geometry.
    public var inputCurves: [InputCurve] {
        let count = area_input_curve_count(ref)
        var result: [InputCurve] = []
        result.reserveCapacity(Int(count))
        for ci in 0..<count {
            let vcount = area_input_vertex_count(ref, ci)
            var vertices: [InputCurve.Vertex] = []
            vertices.reserveCapacity(Int(vcount))
            for vi in 0..<vcount {
                let v = area_input_vertex(ref, ci, vi)
                vertices.append(InputCurve.Vertex(x: v.x, y: v.y, cx: v.cx, cy: v.cy, type: Int(v.type)))
            }
            result.append(InputCurve(vertices: vertices))
        }
        return result
    }

    // MARK: Pocket generation

    /// Generate a pocket toolpath.
    ///
    /// - Parameters:
    ///   - toolRadius: Cutting tool radius.
    ///   - stepover: Radial stepover between passes.
    ///   - mode: Pocket strategy (default: `.spiral`).
    ///   - extraOffset: Additional inward offset applied before pocketing.
    ///   - fromCenter: Start from the centre outward (spiral only).
    ///   - zigAngle: Angle for zig-zag passes in degrees.
    /// - Returns: Array of continuous toolpath curves.
    public func makePocket(
        toolRadius: Double,
        stepover: Double,
        mode: PocketMode = .spiral,
        extraOffset: Double = 0,
        fromCenter: Bool = false,
        zigAngle: Double = 0
    ) -> [PocketCurve] {
        guard let curvesRef = area_make_pocket(
            ref, toolRadius, extraOffset, stepover,
            fromCenter ? 1 : 0, mode.cValue, zigAngle
        ) else { return [] }
        defer { area_curves_free(curvesRef) }

        let curveCount = area_curves_count(curvesRef)
        var result: [PocketCurve] = []
        result.reserveCapacity(Int(curveCount))

        for ci in 0..<curveCount {
            let vcount = area_curve_vertex_count(curvesRef, ci)
            var vertices: [PocketVertex] = []
            vertices.reserveCapacity(Int(vcount))
            for vi in 0..<vcount {
                vertices.append(PocketVertex(area_curve_vertex(curvesRef, ci, vi)))
            }
            result.append(PocketCurve(vertices: vertices))
        }
        return result
    }
}

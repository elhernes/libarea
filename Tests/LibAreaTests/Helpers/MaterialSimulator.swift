import Foundation
import LibArea

// ---------------------------------------------------------------------------
// MARK: - SimulationResult
// ---------------------------------------------------------------------------

struct SimulationResult {
    let coverage: Double
    let totalCuttableCells: Int
    let cutCells: Int
    var uncutCells: Int { totalCuttableCells - cutCells }
    var uncutFraction: Double { 1.0 - coverage }
    let islandViolationCells: Int

    // Internal grid for visualisation.
    // 0 = outside pocket, 1 = cuttable/uncut, 2 = cut,
    // 3 = island material (unviolated), 4 = island material (violated)
    let cells: [UInt8]
    let cols: Int
    let rows: Int
    let origin: Point2D
    let gridResolution: Double
}

// ---------------------------------------------------------------------------
// MARK: - MaterialSimulator
// ---------------------------------------------------------------------------

/// Raster material-removal simulation for LibArea toolpaths.
struct MaterialSimulator {

    let boundary: InputCurve
    let islands: [InputCurve]
    let toolRadius: Double
    let gridResolution: Double

    init(boundary: InputCurve, islands: [InputCurve], toolRadius: Double,
         gridResolution: Double? = nil) {
        self.boundary = boundary
        self.islands = islands
        self.toolRadius = toolRadius
        self.gridResolution = gridResolution ?? max(toolRadius / 5.0, 0.05)
    }

    func simulate(_ curves: [PocketCurve]) -> SimulationResult {
        let res = gridResolution

        // Bounding box from linearized boundary (handles arc extents).
        let linBoundary = boundary.linearized(accuracy: res * 0.5)
        let box = linBoundary.boundingBox

        let ox = box.minX - res
        let oy = box.minY - res
        let cols = max(1, Int(ceil((box.maxX - ox + res) / res)))
        let rows = max(1, Int(ceil((box.maxY - oy + res) / res)))

        // Linearize islands for PIP testing.
        let linIslands = islands.map { $0.linearized(accuracy: res * 0.5) }

        // Phase 1 — classify each cell centre.
        var cells = [UInt8](repeating: 0, count: rows * cols)
        var totalCuttable = 0

        for row in 0..<rows {
            let cy = oy + (Double(row) + 0.5) * res
            for col in 0..<cols {
                let cx = ox + (Double(col) + 0.5) * res
                guard containsPoint(linBoundary, x: cx, y: cy) else { continue }
                var insideIsland = false
                for island in linIslands {
                    if containsPoint(island, x: cx, y: cy) { insideIsland = true; break }
                }
                if insideIsland {
                    cells[row * cols + col] = 3
                } else {
                    cells[row * cols + col] = 1
                    totalCuttable += 1
                }
            }
        }

        // Phase 2 — stamp circles along the toolpath.
        let stampR  = toolRadius + res * 0.5
        let stampR2 = stampR * stampR
        let toolR2  = toolRadius * toolRadius
        let step    = res * 0.5

        func stamp(_ tx: Double, _ ty: Double) {
            let c0 = max(0, Int((tx - stampR - ox) / res))
            let c1 = min(cols - 1, Int((tx + stampR - ox) / res))
            let r0 = max(0, Int((ty - stampR - oy) / res))
            let r1 = min(rows - 1, Int((ty + stampR - oy) / res))
            guard r0 <= r1, c0 <= c1 else { return }

            for r in r0...r1 {
                let dy  = oy + (Double(r) + 0.5) * res - ty
                let dy2 = dy * dy
                if dy2 > stampR2 { continue }
                let maxDx2     = stampR2 - dy2
                let maxViolDx2 = toolR2 > dy2 ? toolR2 - dy2 : -1.0
                for c in c0...c1 {
                    let dx  = ox + (Double(c) + 0.5) * res - tx
                    let dx2 = dx * dx
                    if dx2 <= maxDx2 {
                        let idx = r * cols + c
                        if cells[idx] == 1 { cells[idx] = 2 }
                        else if cells[idx] == 3 && dx2 <= maxViolDx2 { cells[idx] = 4 }
                    }
                }
            }
        }

        for curve in curves {
            guard let start = curve.startPoint else { continue }
            var prev = start
            stamp(prev.x, prev.y)
            for v in curve.vertices.dropFirst() {
                switch v.segmentType {
                case .line:
                    sampleLine(from: prev, to: v.endPoint, step: step, stamp: stamp)
                case .ccwArc(let center):
                    sampleArc(from: prev, to: v.endPoint, center: center, ccw: true,
                              step: step, stamp: stamp)
                case .cwArc(let center):
                    sampleArc(from: prev, to: v.endPoint, center: center, ccw: false,
                              step: step, stamp: stamp)
                }
                prev = v.endPoint
            }
        }

        // Phase 3 — metrics.
        var cutCount = 0, violCount = 0
        for v in cells {
            if v == 2 { cutCount += 1 }
            else if v == 4 { violCount += 1 }
        }

        return SimulationResult(
            coverage: totalCuttable > 0 ? Double(cutCount) / Double(totalCuttable) : 1.0,
            totalCuttableCells: totalCuttable,
            cutCells: cutCount,
            islandViolationCells: violCount,
            cells: cells,
            cols: cols,
            rows: rows,
            origin: Point2D(ox, oy),
            gridResolution: res
        )
    }

    // MARK: - PIP

    /// Ray-casting point-in-polygon (works only on linearized curves).
    private func containsPoint(_ curve: InputCurve, x px: Double, y py: Double) -> Bool {
        let verts = curve.vertices
        guard verts.count >= 3 else { return false }
        var inside = false
        var j = verts.count - 1
        for i in 0..<verts.count {
            let xi = verts[i].x, yi = verts[i].y
            let xj = verts[j].x, yj = verts[j].y
            if ((yi > py) != (yj > py)) && (px < (xj - xi) * (py - yi) / (yj - yi) + xi) {
                inside.toggle()
            }
            j = i
        }
        return inside
    }

    // MARK: - Sampling

    private func sampleLine(from p0: Point2D, to p1: Point2D,
                             step: Double, stamp: (Double, Double) -> Void) {
        let dx = p1.x - p0.x, dy = p1.y - p0.y
        let len = (dx*dx + dy*dy).squareRoot()
        guard len > 1e-10 else { return }
        let n = max(1, Int(ceil(len / step)))
        for i in 1...n {
            let t = Double(i) / Double(n)
            stamp(p0.x + dx * t, p0.y + dy * t)
        }
    }

    private func sampleArc(from start: Point2D, to end: Point2D,
                            center: Point2D, ccw: Bool,
                            step: Double, stamp: (Double, Double) -> Void) {
        let r = (start - center).length
        guard r > 1e-10 else { return }
        let a0 = atan2(start.y - center.y, start.x - center.x)
        let a1 = atan2(end.y   - center.y, end.x   - center.x)
        var sweep = ccw ? (a1 - a0) : (a0 - a1)
        if sweep <= 0 { sweep += 2 * .pi }
        let arcLen = r * sweep
        let n = max(1, Int(ceil(arcLen / step)))
        let dA = (ccw ? 1.0 : -1.0) * sweep / Double(n)
        for i in 1...n {
            let a = a0 + Double(i) * dA
            stamp(center.x + r * cos(a), center.y + r * sin(a))
        }
    }
}

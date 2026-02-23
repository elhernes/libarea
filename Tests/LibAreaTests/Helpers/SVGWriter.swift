import Foundation
import LibArea

enum SVGWriter {

    private static let boundaryColor  = "#2563eb"   // blue
    private static let islandColor    = "#ea580c"   // orange
    private static let toolpathColor  = "#dc2626"   // red
    private static let uncutColor     = "#ef4444"   // red (uncut material)
    private static let violationColor = "#a855f7"   // purple
    private static let strokeWidth    = 0.5

    // MARK: - Public

    static func write(boundary: InputCurve, islands: [InputCurve],
                      toolpath: [PocketCurve], simulation: SimulationResult? = nil,
                      to url: URL) throws {
        // Bounding box from all geometry.
        var mnX = Double.infinity, mnY = Double.infinity
        var mxX = -Double.infinity, mxY = -Double.infinity

        func include(_ x: Double, _ y: Double) {
            if x < mnX { mnX = x }; if x > mxX { mxX = x }
            if y < mnY { mnY = y }; if y > mxY { mxY = y }
        }

        // Use linearized boundary for tight box (handles arc extents).
        for v in boundary.linearized(accuracy: 0.5).vertices { include(v.x, v.y) }
        for island in islands { for v in island.linearized(accuracy: 0.5).vertices { include(v.x, v.y) } }
        for curve in toolpath { for v in curve.vertices { include(v.endPoint.x, v.endPoint.y) } }

        let margin = max(mxX - mnX, mxY - mnY) * 0.05
        mnX -= margin; mnY -= margin; mxX += margin; mxY += margin
        let viewW = mxX - mnX, viewH = mxY - mnY

        var svg = ""
        svg += #"<?xml version="1.0" encoding="UTF-8"?>"# + "\n"
        svg += #"<svg xmlns="http://www.w3.org/2000/svg""#
        svg += #" width="\#(fmt(viewW * 4))" height="\#(fmt(viewH * 4))""#
        svg += #" viewBox="\#(fmt(mnX)) \#(fmt(-mxY)) \#(fmt(viewW)) \#(fmt(viewH))""#
        svg += #" style="background:white">"# + "\n"
        svg += #"<g transform="scale(1,-1)">"# + "\n"

        svg += inputCurveElement(boundary, stroke: boundaryColor, comment: "boundary")
        for (i, island) in islands.enumerated() {
            svg += inputCurveElement(island, stroke: islandColor, comment: "island \(i)")
        }
        for (i, curve) in toolpath.enumerated() {
            svg += pocketCurveElement(curve, stroke: toolpathColor,
                                      opacity: 0.6, strokeWidth: strokeWidth * 0.7,
                                      comment: "curve \(i)")
        }

        if let sim = simulation, sim.uncutCells > 0 || sim.islandViolationCells > 0 {
            svg += simulationLayer(sim)
        }

        svg += "</g>\n"

        // Legend (outside the flipped group, SVG Y is down here)
        let lx = fmt(mnX + margin * 0.3)
        let fs = fmt(margin * 0.5)
        svg += #"<text x="\#(lx)" y="\#(fmt(-mxY + margin * 0.6))" font-size="\#(fs)" fill="\#(boundaryColor)">boundary</text>"# + "\n"
        svg += #"<text x="\#(lx)" y="\#(fmt(-mxY + margin * 1.2))" font-size="\#(fs)" fill="\#(islandColor)">islands</text>"# + "\n"
        svg += #"<text x="\#(lx)" y="\#(fmt(-mxY + margin * 1.8))" font-size="\#(fs)" fill="\#(toolpathColor)">toolpath (\#(toolpath.count) curves)</text>"# + "\n"
        if let sim = simulation {
            let pct = String(format: "%.1f%%", sim.coverage * 100)
            let cc  = sim.coverage >= 0.97 ? "#16a34a" : uncutColor
            svg += #"<text x="\#(lx)" y="\#(fmt(-mxY + margin * 2.4))" font-size="\#(fs)" fill="\#(cc)">coverage: \#(pct)</text>"# + "\n"
            let vc  = sim.islandViolationCells == 0 ? "#16a34a" : violationColor
            svg += #"<text x="\#(lx)" y="\#(fmt(-mxY + margin * 3.0))" font-size="\#(fs)" fill="\#(vc)">island violations: \#(sim.islandViolationCells)</text>"# + "\n"
        }

        svg += "</svg>\n"
        try svg.write(to: url, atomically: true, encoding: .utf8)
    }

    @discardableResult
    static func writeToTmp(name: String, boundary: InputCurve, islands: [InputCurve],
                            toolpath: [PocketCurve], simulation: SimulationResult? = nil) throws -> URL {
        let dir = URL(fileURLWithPath: "/tmp/LibAreaTests")
        try FileManager.default.createDirectory(at: dir, withIntermediateDirectories: true)
        let url = dir.appendingPathComponent("\(name).svg")
        try write(boundary: boundary, islands: islands, toolpath: toolpath,
                  simulation: simulation, to: url)
        if let sim = simulation {
            let pct = String(format: "%.1f%%", sim.coverage * 100)
            print("SVG written:   \(url.path)  coverage: \(pct)")
        } else {
            print("SVG written:   \(url.path)")
        }
        return url
    }

    // MARK: - Simulation overlay

    private static func simulationLayer(_ sim: SimulationResult) -> String {
        var parts = ["<!-- simulation overlay -->", "<g opacity=\"0.55\">"]
        let res = sim.gridResolution, cols = sim.cols

        func emitRuns(value: UInt8, color: String) {
            for row in 0..<sim.rows {
                let y = sim.origin.y + Double(row) * res
                var runStart: Int? = nil
                for col in 0...cols {
                    let matches = col < cols && sim.cells[row * cols + col] == value
                    if matches && runStart == nil {
                        runStart = col
                    } else if !matches, let start = runStart {
                        let x = sim.origin.x + Double(start) * res
                        let w = Double(col - start) * res
                        parts.append("<rect x=\"\(fmt(x))\" y=\"\(fmt(y))\" width=\"\(fmt(w))\" height=\"\(fmt(res))\" fill=\"\(color)\"/>")
                        runStart = nil
                    }
                }
            }
        }

        if sim.uncutCells > 0           { emitRuns(value: 1, color: uncutColor) }
        if sim.islandViolationCells > 0 { emitRuns(value: 4, color: violationColor) }
        parts.append("</g>")
        return parts.joined(separator: "\n") + "\n"
    }

    // MARK: - Path elements

    private static func inputCurveElement(_ curve: InputCurve, stroke: String,
                                           comment: String) -> String {
        let d = inputCurveData(curve)
        return "<!-- \(comment) -->\n" +
            "<path d=\"\(d)\" stroke=\"\(stroke)\" stroke-width=\"\(fmt(strokeWidth))\" fill=\"none\" opacity=\"1\"/>\n"
    }

    private static func pocketCurveElement(_ curve: PocketCurve, stroke: String,
                                            opacity: Double, strokeWidth: Double,
                                            comment: String) -> String {
        let d = pocketCurveData(curve)
        return "<!-- \(comment) -->\n" +
            "<path d=\"\(d)\" stroke=\"\(stroke)\" stroke-width=\"\(fmt(strokeWidth))\" fill=\"none\" opacity=\"\(fmt(opacity))\"/>\n"
    }

    // MARK: - Path data

    private static func inputCurveData(_ curve: InputCurve) -> String {
        guard !curve.vertices.isEmpty else { return "" }
        var parts: [String] = []
        let v0 = curve.vertices[0]
        parts.append("M \(fmtPt(v0.x, v0.y))")
        var curX = v0.x, curY = v0.y
        for v in curve.vertices.dropFirst() {
            if v.type == 0 {
                parts.append("L \(fmtPt(v.x, v.y))")
            } else {
                let r = ((curX - v.cx)*(curX - v.cx) + (curY - v.cy)*(curY - v.cy)).squareRoot()
                let (la, sf) = arcFlags(fromX: curX, fromY: curY, toX: v.x, toY: v.y,
                                        cx: v.cx, cy: v.cy, ccw: v.type == 1)
                parts.append("A \(fmt(r)) \(fmt(r)) 0 \(la) \(sf) \(fmtPt(v.x, v.y))")
            }
            curX = v.x; curY = v.y
        }
        parts.append("Z")
        return parts.joined(separator: " ")
    }

    private static func pocketCurveData(_ curve: PocketCurve) -> String {
        guard let start = curve.startPoint else { return "" }
        var parts: [String] = ["M \(fmtPt(start.x, start.y))"]
        var prev = start
        for v in curve.vertices.dropFirst() {
            switch v.segmentType {
            case .line:
                parts.append("L \(fmtPt(v.endPoint.x, v.endPoint.y))")
            case .ccwArc(let center):
                let r = (prev - center).length
                let (la, sf) = arcFlags(fromX: prev.x, fromY: prev.y,
                                        toX: v.endPoint.x, toY: v.endPoint.y,
                                        cx: center.x, cy: center.y, ccw: true)
                parts.append("A \(fmt(r)) \(fmt(r)) 0 \(la) \(sf) \(fmtPt(v.endPoint.x, v.endPoint.y))")
            case .cwArc(let center):
                let r = (prev - center).length
                let (la, sf) = arcFlags(fromX: prev.x, fromY: prev.y,
                                        toX: v.endPoint.x, toY: v.endPoint.y,
                                        cx: center.x, cy: center.y, ccw: false)
                parts.append("A \(fmt(r)) \(fmt(r)) 0 \(la) \(sf) \(fmtPt(v.endPoint.x, v.endPoint.y))")
            }
            prev = v.endPoint
        }
        return parts.joined(separator: " ")
    }

    // MARK: - Helpers

    /// SVG large-arc-flag and sweep-flag for a circular arc.
    /// `ccw` = counter-clockwise in Y-up coordinates.
    /// We apply `scale(1,-1)` in SVG so Y-up CCW = SVG CCW → sweepFlag = 1.
    private static func arcFlags(fromX: Double, fromY: Double, toX: Double, toY: Double,
                                  cx: Double, cy: Double, ccw: Bool) -> (Int, Int) {
        let a0 = atan2(fromY - cy, fromX - cx)
        let a1 = atan2(toY   - cy, toX   - cx)
        var sweep = ccw ? (a1 - a0) : (a0 - a1)
        if sweep <= 0 { sweep += 2 * .pi }
        let largeArc = abs(sweep) > .pi ? 1 : 0
        let sweepFlag = ccw ? 1 : 0
        return (largeArc, sweepFlag)
    }

    private static func fmt(_ v: Double) -> String { String(format: "%.4f", v) }
    private static func fmtPt(_ x: Double, _ y: Double) -> String { "\(fmt(x)),\(fmt(y))" }
}

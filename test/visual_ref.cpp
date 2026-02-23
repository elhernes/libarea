// visual_ref.cpp
// Reference toolpath generator using libarea, matching the AreaPocket Swift VisualTests.
// Writes SVG files to /tmp/AreaPocketRef/ for visual comparison.
//
// Geometries reproduced:
//   circle_center_island   — circle r=40 + concentric island r=12
//   circle_two_islands     — circle r=40 + two off-centre islands
//   peanut_no_island       — union of two circles r=28
//   peanut_island          — same peanut + island r=9 at waist

#include "../src/Area.h"
#include "../src/Curve.h"
#include <stdio.h>
#include <math.h>
#include <string>
#include <sstream>
#include <algorithm>
#include <sys/stat.h>

static const double ACCURACY    = 0.01;
static const double TOOL_RADIUS = 3.0;
static const double STEPOVER    = 2.5;

// ---------------------------------------------------------------
// Geometry helpers
// ---------------------------------------------------------------

// Two-arc CCW circle (open at right, closes at right)
static void makeCircle(CCurve& c, const Point& center, double radius) {
    c.append(center + Point(radius, 0));   // start at (cx+r, cy)
    c.append(CVertex(CVertex::vt_ccw_arc, center + Point(-radius, 0), center));
    c.append(CVertex(CVertex::vt_ccw_arc, center + Point( radius, 0), center));
}

static void makeRect(CCurve& c, const Point& p0, double w, double h) {
    c.append(p0);
    c.append(p0 + Point(w, 0));
    c.append(p0 + Point(w, h));
    c.append(p0 + Point(0, h));
    c.append(p0);
}

static CAreaPocketParams spiralParams() {
    return CAreaPocketParams(TOOL_RADIUS, 0.0, STEPOVER,
                             false, PocketMode::Spiral, 0.0);
}

// ---------------------------------------------------------------
// SVG writing
// ---------------------------------------------------------------

// large-arc-flag: 0 if sweep ≤ π, 1 if sweep > π
static int largeArcFlag(const Point& from, const CVertex& v) {
    double sa = std::atan2(from.y - v.m_c.y, from.x - v.m_c.x);
    double ea = std::atan2(v.m_p.y - v.m_c.y, v.m_p.x - v.m_c.x);
    bool ccw = (v.m_type == CVertex::vt_ccw_arc);
    double sweep = ccw ? (ea - sa) : (sa - ea);
    if (sweep <= 0) sweep += 2 * M_PI;
    return (sweep > M_PI) ? 1 : 0;
}

// SVG path string for a closed CCurve
static std::string closedPath(const CCurve& c) {
    std::ostringstream s;
    bool first = true;
    Point prev;
    for (const auto& v : c.m_vertices) {
        if (first) {
            s << "M " << v.m_p.x << "," << v.m_p.y;
            prev = v.m_p; first = false; continue;
        }
        switch (v.m_type) {
        case CVertex::vt_line:
            s << " L " << v.m_p.x << "," << v.m_p.y;
            break;
        case CVertex::vt_ccw_arc: {
            double r = std::sqrt((prev.x-v.m_c.x)*(prev.x-v.m_c.x)+(prev.y-v.m_c.y)*(prev.y-v.m_c.y));
            s << " A " << r << " " << r << " 0 " << largeArcFlag(prev,v) << " 1 " << v.m_p.x << "," << v.m_p.y;
            break; }
        case CVertex::vt_cw_arc: {
            double r = std::sqrt((prev.x-v.m_c.x)*(prev.x-v.m_c.x)+(prev.y-v.m_c.y)*(prev.y-v.m_c.y));
            s << " A " << r << " " << r << " 0 " << largeArcFlag(prev,v) << " 0 " << v.m_p.x << "," << v.m_p.y;
            break; }
        }
        prev = v.m_p;
    }
    s << " Z";
    return s.str();
}

// SVG path string for an open toolpath CCurve
static std::string openPath(const CCurve& c) {
    std::ostringstream s;
    bool first = true;
    Point prev;
    for (const auto& v : c.m_vertices) {
        if (first) {
            s << "M " << v.m_p.x << "," << v.m_p.y;
            prev = v.m_p; first = false; continue;
        }
        switch (v.m_type) {
        case CVertex::vt_line:
            s << " L " << v.m_p.x << "," << v.m_p.y;
            break;
        case CVertex::vt_ccw_arc: {
            double r = std::sqrt((prev.x-v.m_c.x)*(prev.x-v.m_c.x)+(prev.y-v.m_c.y)*(prev.y-v.m_c.y));
            s << " A " << r << " " << r << " 0 " << largeArcFlag(prev,v) << " 1 " << v.m_p.x << "," << v.m_p.y;
            break; }
        case CVertex::vt_cw_arc: {
            double r = std::sqrt((prev.x-v.m_c.x)*(prev.x-v.m_c.x)+(prev.y-v.m_c.y)*(prev.y-v.m_c.y));
            s << " A " << r << " " << r << " 0 " << largeArcFlag(prev,v) << " 0 " << v.m_p.x << "," << v.m_p.y;
            break; }
        }
        prev = v.m_p;
    }
    return s.str();
}

static void writeSVG(const char* path,
                     const CArea& area,
                     const std::list<CCurve>& toolpath) {
    // Bounding box (vertex-based; good enough for these geometries)
    double minX=1e18, minY=1e18, maxX=-1e18, maxY=-1e18;
    auto extendBB = [&](double x, double y){
        minX=std::min(minX,x); minY=std::min(minY,y);
        maxX=std::max(maxX,x); maxY=std::max(maxY,y);
    };
    for (const auto& c : area.m_curves)
        for (const auto& v : c.m_vertices)
            extendBB(v.m_p.x, v.m_p.y);
    for (const auto& c : toolpath)
        for (const auto& v : c.m_vertices)
            extendBB(v.m_p.x, v.m_p.y);

    double pad = std::max(maxX-minX, maxY-minY) * 0.07 + 5.0;
    double vx=minX-pad, vy=minY-pad, vw=maxX-minX+2*pad, vh=maxY-minY+2*pad;
    double scale = 4.0;

    FILE* f = fopen(path, "w");
    if (!f) { fprintf(stderr, "Cannot open %s\n", path); return; }

    fprintf(f, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
               "<svg xmlns=\"http://www.w3.org/2000/svg\" "
               "width=\"%.0f\" height=\"%.0f\" "
               "viewBox=\"%.4f %.4f %.4f %.4f\" style=\"background:white\">\n",
            vw*scale, vh*scale, vx, -(vy+vh), vw, vh);
    fprintf(f, "<g transform=\"scale(1,-1)\">\n");

    bool first = true; int idx = 0;
    for (const auto& c : area.m_curves) {
        if (first) {
            fprintf(f, "<!-- boundary -->\n"
                       "<path d=\"%s\" stroke=\"#2563eb\" stroke-width=\"0.5\" fill=\"none\" opacity=\"1\"/>\n",
                    closedPath(c).c_str());
            first = false;
        } else {
            fprintf(f, "<!-- island %d -->\n"
                       "<path d=\"%s\" stroke=\"#ea580c\" stroke-width=\"0.5\" fill=\"none\" opacity=\"1\"/>\n",
                    idx++, closedPath(c).c_str());
        }
    }
    for (const auto& c : toolpath)
        fprintf(f, "<!-- toolpath -->\n"
                   "<path d=\"%s\" stroke=\"#dc2626\" stroke-width=\"0.35\" fill=\"none\" opacity=\"0.6\"/>\n",
                openPath(c).c_str());

    fprintf(f, "</g>\n</svg>\n");
    fclose(f);
    fprintf(stderr, "SVG written: %s\n", path);
}

// ---------------------------------------------------------------
// Test cases
// ---------------------------------------------------------------

static void testCircleCenterIsland() {
    CArea area(ACCURACY);
    CCurve boundary; makeCircle(boundary, Point(50,50), 40);
    area.append(boundary);
    CCurve island; makeCircle(island, Point(50,50), 12); island.Reverse();
    area.append(island);
    area.Reorder();

    std::list<CCurve> tp;
    CAreaProcessingContext ctx; ctx.fit_arcs = true;
    area.SplitAndMakePocketToolpath(tp, spiralParams(), &ctx);
    writeSVG("/tmp/AreaPocketRef/circle_center_island.svg", area, tp);
}

static void testCircleTwoIslands() {
    CArea area(ACCURACY);
    CCurve boundary; makeCircle(boundary, Point(50,50), 40);
    area.append(boundary);
    CCurve i1; makeCircle(i1, Point(30,55), 9); i1.Reverse(); area.append(i1);
    CCurve i2; makeCircle(i2, Point(65,38), 11); i2.Reverse(); area.append(i2);
    area.Reorder();

    std::list<CCurve> tp;
    CAreaProcessingContext ctx; ctx.fit_arcs = true;
    area.SplitAndMakePocketToolpath(tp, spiralParams(), &ctx);
    writeSVG("/tmp/AreaPocketRef/circle_two_islands.svg", area, tp);
}

static void testPeanutNoIsland() {
    CArea a1(ACCURACY); CCurve c1; makeCircle(c1, Point(35,50), 28); a1.append(c1);
    CArea a2(ACCURACY); CCurve c2; makeCircle(c2, Point(65,50), 28); a2.append(c2);
    a1.Union(a2);
    a1.FitArcs();

    std::list<CCurve> tp;
    CAreaProcessingContext ctx; ctx.fit_arcs = true;
    a1.SplitAndMakePocketToolpath(tp, spiralParams(), &ctx);
    writeSVG("/tmp/AreaPocketRef/peanut_no_island.svg", a1, tp);
}

static void testPeanutWithIsland() {
    CArea a1(ACCURACY); CCurve c1; makeCircle(c1, Point(35,50), 28); a1.append(c1);
    CArea a2(ACCURACY); CCurve c2; makeCircle(c2, Point(65,50), 28); a2.append(c2);
    a1.Union(a2);
    a1.FitArcs();

    CCurve island; makeCircle(island, Point(50,50), 9); island.Reverse();
    a1.m_curves.push_back(island);
    a1.Reorder();

    std::list<CCurve> tp;
    CAreaProcessingContext ctx; ctx.fit_arcs = true;
    a1.SplitAndMakePocketToolpath(tp, spiralParams(), &ctx);
    writeSVG("/tmp/AreaPocketRef/peanut_island.svg", a1, tp);
}

// ---------------------------------------------------------------
int main() {
    mkdir("/tmp/AreaPocketRef", 0755);
    fprintf(stderr, "libarea visual reference tests\n");
    testCircleCenterIsland();
    testCircleTwoIslands();
    testPeanutNoIsland();
    testPeanutWithIsland();
    fprintf(stderr, "Done.\n");
    return 0;
}

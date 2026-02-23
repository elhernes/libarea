// area_c.cpp — C shim implementing area_c.h over the libarea C++ API.

#include "include/area_c.h"
#include "Area.h"
#include "Curve.h"

#include <vector>
#include <list>

// ---------------------------------------------------------------------------
// Internal C++ structs behind the opaque C handles
// ---------------------------------------------------------------------------

struct _AreaState {
    CArea   area;
    CCurve  pending;    // curve being built by area_begin/add/end_curve
    bool    has_pending = false;

    explicit _AreaState(double accuracy) : area(accuracy) {}
};

struct _CurvesResult {
    struct Curve {
        std::vector<AreaVertexInfo> vertices;
    };
    std::vector<Curve> curves;
};

// ---------------------------------------------------------------------------
// Area lifetime
// ---------------------------------------------------------------------------

extern "C" AreaRef area_create(double accuracy) {
    return new _AreaState(accuracy);
}

extern "C" void area_free(AreaRef area) {
    delete area;
}

// ---------------------------------------------------------------------------
// Building geometry
// ---------------------------------------------------------------------------

extern "C" void area_begin_curve(AreaRef area) {
    area->pending = CCurve();
    area->has_pending = true;
}

extern "C" void area_add_line_vertex(AreaRef area, double x, double y) {
    area->pending.append(CVertex(Point(x, y)));
}

extern "C" void area_add_arc_vertex(AreaRef area, double x, double y,
                                     double cx, double cy, int is_ccw) {
    CVertex::Type t = is_ccw ? CVertex::vt_ccw_arc : CVertex::vt_cw_arc;
    area->pending.append(CVertex(t, Point(x, y), Point(cx, cy)));
}

extern "C" void area_end_curve(AreaRef area) {
    if (area->has_pending) {
        area->area.append(area->pending);
        area->pending = CCurve();
        area->has_pending = false;
    }
}

// ---------------------------------------------------------------------------
// Pocket toolpath generation
// ---------------------------------------------------------------------------

extern "C" AreaCurvesRef area_make_pocket(AreaRef area,
                                           double tool_radius,
                                           double extra_offset,
                                           double stepover,
                                           int from_center,
                                           AreaPocketMode mode,
                                           double zig_angle) {
    PocketMode pm = static_cast<PocketMode>(mode);
    CAreaPocketParams params(tool_radius, extra_offset, stepover,
                             from_center != 0, pm, zig_angle);

    std::list<CCurve> toolpath;
    area->area.SplitAndMakePocketToolpath(toolpath, params);

    auto* result = new _CurvesResult();
    for (const auto& curve : toolpath) {
        _CurvesResult::Curve cd;
        for (const auto& v : curve.m_vertices) {
            AreaVertexInfo vi;
            vi.x    = v.m_p.x;
            vi.y    = v.m_p.y;
            vi.cx   = v.m_c.x;
            vi.cy   = v.m_c.y;
            vi.type = static_cast<int>(v.m_type);
            cd.vertices.push_back(vi);
        }
        result->curves.push_back(std::move(cd));
    }
    return result;
}

extern "C" void area_curves_free(AreaCurvesRef curves) {
    delete curves;
}

// ---------------------------------------------------------------------------
// Boolean operations
// ---------------------------------------------------------------------------

extern "C" void area_unite(AreaRef target, AreaRef other) {
    target->area.Union(other->area);
}

extern "C" void area_subtract(AreaRef target, AreaRef other) {
    target->area.Subtract(other->area);
}

// ---------------------------------------------------------------------------
// Reading back input curves
// ---------------------------------------------------------------------------

static const CCurve* inputCurveAt(AreaRef area, int idx) {
    if (idx < 0) return nullptr;
    auto it = area->area.m_curves.begin();
    for (int i = 0; i < idx; ++i) {
        if (it == area->area.m_curves.end()) return nullptr;
        ++it;
    }
    return (it != area->area.m_curves.end()) ? &(*it) : nullptr;
}

extern "C" int area_input_curve_count(AreaRef area) {
    return static_cast<int>(area->area.m_curves.size());
}

extern "C" int area_input_vertex_count(AreaRef area, int curve_idx) {
    const CCurve* c = inputCurveAt(area, curve_idx);
    if (!c) return 0;
    return static_cast<int>(c->m_vertices.size());
}

extern "C" AreaVertexInfo area_input_vertex(AreaRef area, int curve_idx, int vertex_idx) {
    AreaVertexInfo zero{};
    const CCurve* c = inputCurveAt(area, curve_idx);
    if (!c) return zero;
    if (vertex_idx < 0 || vertex_idx >= static_cast<int>(c->m_vertices.size())) return zero;
    auto it = c->m_vertices.begin();
    std::advance(it, vertex_idx);
    AreaVertexInfo vi;
    vi.x    = it->m_p.x;
    vi.y    = it->m_p.y;
    vi.cx   = it->m_c.x;
    vi.cy   = it->m_c.y;
    vi.type = static_cast<int>(it->m_type);
    return vi;
}

// ---------------------------------------------------------------------------
// Reading result curves
// ---------------------------------------------------------------------------

extern "C" int area_curves_count(AreaCurvesRef curves) {
    return static_cast<int>(curves->curves.size());
}

extern "C" int area_curve_vertex_count(AreaCurvesRef curves, int curve_idx) {
    if (curve_idx < 0 || curve_idx >= static_cast<int>(curves->curves.size()))
        return 0;
    return static_cast<int>(curves->curves[curve_idx].vertices.size());
}

extern "C" AreaVertexInfo area_curve_vertex(AreaCurvesRef curves,
                                             int curve_idx, int vertex_idx) {
    AreaVertexInfo zero{};
    if (curve_idx < 0 || curve_idx >= static_cast<int>(curves->curves.size()))
        return zero;
    const auto& cd = curves->curves[curve_idx];
    if (vertex_idx < 0 || vertex_idx >= static_cast<int>(cd.vertices.size()))
        return zero;
    return cd.vertices[vertex_idx];
}

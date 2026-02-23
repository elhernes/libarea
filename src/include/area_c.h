// area_c.h — C interface to libarea for Swift/SwiftPM consumption.
// Copyright 2011, Dan Heeks (libarea) / Swift bridge additions by Eric Hernes.
// Released under the BSD license. See COPYING for details.

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// ---------------------------------------------------------------------------
// Opaque handle types
// ---------------------------------------------------------------------------

struct _AreaState;
typedef struct _AreaState* AreaRef;

struct _CurvesResult;
typedef struct _CurvesResult* AreaCurvesRef;

// ---------------------------------------------------------------------------
// Vertex data returned from result queries
// ---------------------------------------------------------------------------

typedef struct {
    double x, y;    // end point
    double cx, cy;  // arc center (only meaningful when type != 0)
    int type;       // 0 = line, 1 = CCW arc, -1 = CW arc
} AreaVertexInfo;

// ---------------------------------------------------------------------------
// Pocket mode
// ---------------------------------------------------------------------------

typedef enum {
    AREA_POCKET_SPIRAL                   = 0,
    AREA_POCKET_ZIGZAG                   = 1,
    AREA_POCKET_SINGLE_OFFSET            = 2,
    AREA_POCKET_ZIGZAG_THEN_SINGLE_OFFSET = 3,
} AreaPocketMode;

// ---------------------------------------------------------------------------
// Area lifetime
// ---------------------------------------------------------------------------

/// Create a new empty area.  accuracy controls clipper/arc-fitting tolerance.
AreaRef area_create(double accuracy);

/// Free an area previously created with area_create.
void area_free(AreaRef area);

// ---------------------------------------------------------------------------
// Building the area geometry (boundary + islands)
// ---------------------------------------------------------------------------

/// Begin accumulating a new curve.  Call before adding any vertices.
void area_begin_curve(AreaRef area);

/// Add a line vertex (end point of a line segment from the previous vertex).
/// For the very first call after area_begin_curve this sets the start point.
void area_add_line_vertex(AreaRef area, double x, double y);

/// Add an arc vertex.  (x, y) is the end point; (cx, cy) is the arc centre.
/// is_ccw: 1 = counter-clockwise, 0 = clockwise.
void area_add_arc_vertex(AreaRef area, double x, double y,
                          double cx, double cy, int is_ccw);

/// Commit the current curve to the area.
void area_end_curve(AreaRef area);

// ---------------------------------------------------------------------------
// Pocket toolpath generation
// ---------------------------------------------------------------------------

/// Generate a pocket toolpath.
/// Returns an opaque handle to the result curve list; caller must free it
/// with area_curves_free().  Returns NULL on failure.
AreaCurvesRef area_make_pocket(AreaRef area,
                                double tool_radius,
                                double extra_offset,
                                double stepover,
                                int from_center,
                                AreaPocketMode mode,
                                double zig_angle);

/// Free a curves result previously returned by area_make_pocket.
void area_curves_free(AreaCurvesRef curves);

// ---------------------------------------------------------------------------
// Boolean operations (modify target area in place)
// ---------------------------------------------------------------------------

/// target = target ∪ other
void area_unite(AreaRef target, AreaRef other);

/// target = target − other
void area_subtract(AreaRef target, AreaRef other);

// ---------------------------------------------------------------------------
// Reading back the area's input curves after construction / boolean ops
// ---------------------------------------------------------------------------

/// Number of curves currently stored in the area.
int area_input_curve_count(AreaRef area);

/// Number of vertices in one of the area's stored curves.
int area_input_vertex_count(AreaRef area, int curve_idx);

/// Vertex data from one of the area's stored curves.
/// vertex 0 is the start point (type == 0).
AreaVertexInfo area_input_vertex(AreaRef area, int curve_idx, int vertex_idx);

// ---------------------------------------------------------------------------
// Reading back result curves
// ---------------------------------------------------------------------------

/// Number of curves in the result.
int area_curves_count(AreaCurvesRef curves);

/// Number of vertices in a specific result curve (includes the start vertex).
int area_curve_vertex_count(AreaCurvesRef curves, int curve_idx);

/// Retrieve one vertex.  The first vertex (index 0) is the curve start point
/// and always has type == 0.  Subsequent vertices describe path segments.
AreaVertexInfo area_curve_vertex(AreaCurvesRef curves, int curve_idx, int vertex_idx);

#ifdef __cplusplus
}
#endif

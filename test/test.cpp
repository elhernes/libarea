/***************************************************
 * file: /Users/eric/work/github/heeks/libarea/test.cpp
 *
 * @file    test.cpp
 * @author  Eric L. Hernes
 * @version V1.0
 * @born_on   Saturday, January 21, 2017
 * @copyright (C) Copyright Eric L. Hernes 2017
 * @copyright (C) Copyright Q, Inc. 2017
 *
 * @brief   An Eric L. Hernes Signature Series C++ module
 *
 */

#include "../src/Area.h"
#include "../src/Curve.h"
#include "../src/Point.h"
#include "Writer.h"
#include <stdio.h>

#include <math.h>

#define VT_LINE CVertex::vt_line
#define VT_CCW_ARC CVertex::vt_ccw_arc
#define VT_CW_ARC CVertex::vt_cw_arc

void
dump_clist(const char *tag, const std::list<CCurve> &clist) {
  unsigned cx=0;
  for(const auto &c : clist) {
    unsigned vx=0;
    for(const auto &v : c.m_vertices) {
      printf("%s(%d:%d): (type %d) (xy %f %f)\n", tag, cx, vx, v.m_type, v.m_p.x, v.m_p.y);
      vx++;
    }
    cx++;
  }
}

static bool
makeCircle(CCurve &curve, const Point &center, double radius) {
  curve.append(center + Point(radius, 0));
  curve.append(CVertex(CVertex::vt_ccw_arc, center + Point(-radius, 0), center));
  curve.append(CVertex(CVertex::vt_ccw_arc, center + Point(-radius, 0), center));
  curve.append(CVertex(CVertex::vt_ccw_arc, center + Point(radius, 0), center));
  return true;
}

static bool
makeRect(CCurve &curve, const Point &p0, double xl, double yl) {
  curve.append(p0);
  curve.append(p0 + Point(xl, 0));
  curve.append(p0 + Point(xl, yl));
  curve.append(p0 + Point(0, yl));
  curve.append(p0);
  return true;
}

static bool
makeTriangle(CCurve &curve, const Point &p0, const Point &p1, const Point &p2) {
  curve.append(p0);
  curve.append(p1);
  curve.append(p2);
  curve.append(p0);
  return true;
}

static void
cut_path(Writer &out, std::list<CCurve> &toolpath, double depth, double zStart,
         double zSafe) {
  for (std::list<CCurve>::const_iterator i = toolpath.begin();
       i != toolpath.end(); i++) {
    const CCurve &c = *i;

    double px = 0.0;
    double py = 0.0;
    bool firstPoint = true;
    for (std::list<CVertex>::const_iterator vi = c.m_vertices.begin();
         vi != c.m_vertices.end(); vi++) {
      const CVertex &v = *vi;
      if (firstPoint) {
        // rapid across
        out.OnRapidXY(v.m_p.x, v.m_p.y);

        // rapid down
        out.OnRapidZ(zStart);

        // feed down
        out.OnFeedZ(depth);
      } else {
        if (v.m_type == 1) {
          out.OnArcCCW(v.m_p.x, v.m_p.y, v.m_c.x - px, v.m_c.y - py);

        } else if (v.m_type == -1) {
          out.OnArcCW(v.m_p.x, v.m_p.y, v.m_c.y - px, v.m_c.y - py);
        } else {
          out.OnFeedXY(v.m_p.x, v.m_p.y);
        }
      }
      px = v.m_p.x;
      py = v.m_p.y;
      firstPoint = false;
    }
    out.OnRapidZ(zSafe);
  }
}

void
circular_pocket(std::list<CCurve> &toolPath, double tool_diameter, double cx,
                double cy, double rOuter, double rInner, double xyPct, const Units &u) {
  CCurve outerCircle, innerCircle;
  CArea innerArea(u), outerArea(u);

  makeCircle(outerCircle, Point(cx, cy), rOuter);
  outerArea.append(outerCircle);
  outerCircle.FitArcs(u);

  makeCircle(innerCircle, Point(cx, cy), rInner);
  innerArea.append(innerCircle);
  innerCircle.FitArcs(u);

  outerArea.Xor(innerArea);

  PocketMode pm = SpiralPocketMode;
//  PocketMode pm = ZigZagPocketMode;
//  PocketMode pm = SingleOffsetPocketMode;
//  PocketMode pm = ZigZagThenSingleOffsetPocketMode;
  
  CAreaPocketParams params(tool_diameter / 2,
                           0,                     // double Extra_offset
                           tool_diameter * xyPct, // double Stepover,
                           false,                 // bool From_center,
                           pm, // PocketMode Mode,
                           0); // double Zig_angle)
  outerArea.SplitAndMakePocketToolpath(toolPath, params);
}

void
rect_pocket(std::list<CCurve> &toolPath, double tool_diameter, double x0,
            double y0, double xlen, double ylen, double xyPct, const Units &u) {
  CCurve rect_c;
  makeRect(rect_c, Point(x0, y0), xlen, ylen);
  CArea rect_a(u);
  rect_a.append(rect_c);

  PocketMode pm = SpiralPocketMode;
//  PocketMode pm = ZigZagPocketMode;
//  PocketMode pm = SingleOffsetPocketMode;
//  PocketMode pm = ZigZagThenSingleOffsetPocketMode;
  
  CAreaPocketParams params(tool_diameter / 2,
                           0,                     // double Extra_offset
                           tool_diameter * xyPct, // double Stepover,
                           false,                 // bool From_center,
                           pm,      // PocketMode Mode,
                           22.5);                    // double Zig_angle)
  rect_a.MakePocketToolpath(toolPath, params);
}

void
triangle_pocket(std::list<CCurve> &toolPath, double tool_diameter,
                double x0, double y0,
                double x1, double y1,
                double x2, double y2,
                double xyPct, const Units &u) {
  
  CCurve tri_c;
  makeTriangle(tri_c, Point(x0, y0), Point(x1,y1), Point(x2,y2));

  unsigned i=0;
    for(const auto &v : tri_c.m_vertices) {
      fprintf(stderr, "point[%d] (%f %f)\n", i, v.m_p.x, v.m_p.y);
      i++;
  }

  CArea tri_a(u);
  tri_a.append(tri_c);

//  PocketMode pm = SpiralPocketMode;
//  PocketMode pm = ZigZagPocketMode;
//  PocketMode pm = SingleOffsetPocketMode;
  PocketMode pm = ZigZagThenSingleOffsetPocketMode;

  fprintf(stderr, "pocket mode is %d\n", pm);
  
  CAreaPocketParams params(tool_diameter / 2,
                           0,                     // double Extra_offset
                           tool_diameter * xyPct, // double Stepover,
                           false,                 // bool From_center,
                           pm,      // PocketMode Mode,
                           0);                    // double Zig_angle)

  tri_a.MakePocketToolpath(toolPath, params);
}

static void
polygon_pocket(std::list<CCurve> &toolPath,
               double tool_diameter, PocketMode pm, double zz_angle, double xyPct,
               const CCurve &poly_c, const Units &u) {

  CArea poly_a(u);
  poly_a.append(poly_c);
  dump_clist("poly_a", poly_a.m_curves);

  CAreaPocketParams params(tool_diameter / 2,
                           0,                     // double Extra_offset
                           tool_diameter * xyPct, // double Stepover,
                           false,                 // bool From_center,
                           pm,      // PocketMode Mode,
                           zz_angle);                    // double Zig_angle)

  poly_a.MakePocketToolpath(toolPath, params);
}


int
main(int ac, char **av) {
  Point p(0, 0);

  double dm[] = {1., 0., 0., 12., 0., 1., 0., 0.,
                 0., 0., 1., 0.,  0., 0., 0., 1.};
  geoff_geometry::Matrix m(dm);
  p.Transform(m);
  printf("p(x=%f, y=%f)\n", p.x, p.y);

  Units u(25.4, 0.001);
  std::list<CCurve> toolPath;

  GCodeWriter gcode("pocket.nc", 0);
    if (/* DISABLES CODE */ (0)) {
    circular_pocket(toolPath, 0.125/*tool diameter*/,
                    1.5, 1.5, /* cx,cy */
                    1.5, /* outer radius */
                    0.75, /* inner radius */
                    0.45, u); /* xy overlap */

    cut_path(gcode, toolPath, -0.125, 0., 0.125);
  }

  if (/* DISABLES CODE */ (0)) {
    toolPath.clear();
    rect_pocket(toolPath, 0.125, /*tool diameter*/
                1.5, 1.5, // bottom left x, y
                1.50, 3.50, // width, height
                0.45, u);  // xy overlap,  units

    cut_path(gcode, toolPath, -0.250, 0., 0.125);
  }
  
    if (/* DISABLES CODE */ (0)) {
    toolPath.clear();
    triangle_pocket(toolPath, 10./25.4, /*tool diameter*/
                    2.693, -0.663, // p2
                    1.854, 0.175, // p1
                    1.016, -0.663, // p0
                    0.45, u);  // xy overlap,  units

    fprintf(stderr, "curves: %lu\n", toolPath.size());
    for (const auto &c : toolPath) {
        fprintf(stderr, "vertices: %lu\n", c.m_vertices.size());
          for (const auto &cv : c.m_vertices) {
          fprintf(stderr, "  point: (%f %f)\n", cv.m_p.x, cv.m_p.y);
        }
    }

    cut_path(gcode, toolPath, -0.500, -0.500, -1.0);
  }

  if (1) {
    toolPath.clear();
    CCurve poly_c;

    int layer = 4;

    if (layer == 1) { // Layer1 - slant w/ goiters
      poly_c.append(Point(1.5, 2.150));
      poly_c.append(CVertex(VT_CCW_ARC, Point(1.067,2.150), Point(1.283,1.283)));
      poly_c.append(Point(0, 2.150));
      poly_c.append(Point(0, 0.650));
      poly_c.append(CVertex(VT_CCW_ARC, Point(0.650,0), Point(0.650,0.650)));
      poly_c.append(Point(2.150, 0));
      poly_c.append(Point(2.150, 1.067));
      poly_c.append(CVertex(VT_CCW_ARC, Point(2.150,1.5), Point(2.150,2.150)));
      poly_c.append(Point(1.5, 2.150));    }

    if (layer == 2) { // Layer2 - actual part
      poly_c.append(Point(0, 0.650));
      poly_c.append(CVertex(VT_CCW_ARC, Point(0.650,0), Point(0.650,0.650)));
      poly_c.append(Point(2.150, 0));
      poly_c.append(Point(2.150, 1.067));
      poly_c.append(Point(2.150, 1.5));
      poly_c.append(CVertex(VT_CW_ARC, Point(1.5,2.150), Point(2.150,2.150)));
      poly_c.append(Point(1.067, 2.150));
      poly_c.append(Point(0, 2.150));
      poly_c.append(Point(0, 0.650));      
    }

    if (layer == 3) { // Layer3 - part w/ goiters
    
      poly_c.append(Point(1.5, 2.150));
      poly_c.append(CVertex(VT_CCW_ARC, Point(1.067,2.150), Point(1.283,1.283)));
      poly_c.append(Point(0, 2.150));
      poly_c.append(Point(0, 0.650));
      poly_c.append(CVertex(VT_CCW_ARC, Point(0.650,0), Point(0.650,0.650)));
      poly_c.append(Point(2.150, 0));
      poly_c.append(Point(2.150, 1.067));
      poly_c.append(CVertex(VT_CCW_ARC, Point(2.150,1.5), Point(2.150,2.150)));
      poly_c.append(CVertex(VT_CW_ARC, Point(1.50,2.150), Point(2.150,2.150)));
    }

    if (layer == 4) { // Layer1 - slant w/ goiters
      poly_c.append(Point(1.5, 2.150000000000003));
      poly_c.append(CVertex(CVertex::vt_ccw_arc, Point(1.066929133858268,2.150000000000003), Point(1.283464566929134,1.283464566929134)));
      poly_c.append(Point(9e-16, 2.150000000000002));
      poly_c.append(Point(-9e-16, 0.6500000000000018));
      poly_c.append(CVertex(CVertex::vt_ccw_arc, Point(0.6499999999999998,1.4432899320127035e-15), Point(0.6499999999999999,0.6499999999999999)));
      poly_c.append(Point(2.149999999999999, 4e-16));
      poly_c.append(Point(2.149999999999999, 1.066929133858268));
      poly_c.append(CVertex(CVertex::vt_ccw_arc, Point(2.149999999999993,1.5), Point(2.149999999999993,2.149999999999993)));
      poly_c.append(Point(1.5, 2.150000000000001));    }

    if (layer == 5) { // Layer2 - actual part
      poly_c.append(Point(1.1102230246251565e-16, 0.650000000000008));
      poly_c.append(CVertex(CVertex::vt_ccw_arc, Point(0.6499999999999998,1.4432899320127035e-15), Point(0.6499999999999999,0.6499999999999999)));
      poly_c.append(Point(2.149999999999999, 4e-16));
      poly_c.append(Point(2.149999999999999, 1.066929133858268));
      poly_c.append(Point(2.149999999999993, 1.5));
      poly_c.append(CVertex(CVertex::vt_cw_arc, Point(1.4999999999999996,2.1500000000000075), Point(2.150000000000001,2.150000000000001)));
      poly_c.append(Point(1.066929133858268, 2.150000000000002));
      poly_c.append(Point(9e-16, 2.150000000000002));
      poly_c.append(Point(-9e-16, 0.6500000000000018));      
    }

    if (layer == 6) { // Layer3 - part w/ goiters
    
      poly_c.append(Point(1.5, 2.150000000000003));
      poly_c.append(CVertex(CVertex::vt_ccw_arc, Point(1.066929133858268,2.150000000000003), Point(1.283464566929134,1.283464566929134)));
      poly_c.append(Point(9e-16, 2.150000000000002));
      poly_c.append(Point(-9e-16, 0.6500000000000018));
      poly_c.append(CVertex(CVertex::vt_ccw_arc, Point(0.6499999999999998,1.4432899320127035e-15), Point(0.6499999999999999,0.6499999999999999)));
      poly_c.append(Point(2.149999999999999, 4e-16));
      poly_c.append(Point(2.149999999999999, 1.066929133858268));
      poly_c.append(CVertex(CVertex::vt_ccw_arc, Point(2.149999999999993,1.5), Point(2.149999999999993,2.149999999999993)));
      poly_c.append(CVertex(CVertex::vt_cw_arc, Point(1.4999999999999996,2.1500000000000075), Point(2.150000000000001,2.150000000000001)));
    }


    polygon_pocket(toolPath,
                   10./25.4, /*tool diameter*/
                   SpiralPocketMode, /* mode */
                   0., // zz_angle
                   0.45, // xy-pct
                   poly_c,
                   Units(25.4, 0.001));

    fprintf(stderr, "curves: %lu\n", toolPath.size());

    cut_path(gcode, toolPath, -0.500, -0.500, 1.0);
  }

  return 0;
}

/* end of /Users/eric/work/github/heeks/libarea/test.cpp */

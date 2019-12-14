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

#include "Area.h"
#include "Curve.h"
#include "Point.h"
#include "Writer.h"
#include <stdio.h>

#include <math.h>

static bool
makeCircle(CCurve &curve, const Point &center, double radius) {
  curve.append(center + Point(radius, 0));
  curve.append(CVertex(1, center + Point(-radius, 0), center));
  curve.append(CVertex(1, center + Point(-radius, 0), center));
  curve.append(CVertex(1, center + Point(radius, 0), center));
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

static void
cut_area(Writer &out, std::list<CCurve> &toolpath, double depth, double zStart,
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
                double cy, double rOuter, double rInner, double xyPct) {
  CCurve outerCircle, innerCircle;
  CArea innerArea, outerArea;

  makeCircle(outerCircle, Point(cx, cy), rOuter);
  outerArea.append(outerCircle);
//  outerCircle.FitArcs(Units(Units::Inches, 0.001));

  makeCircle(innerCircle, Point(cx, cy), rInner);
  innerArea.append(innerCircle);
//  innerCircle.FitArcs(Units(Units::Inches, 0.001));

  outerArea.Xor(innerArea);

  // SpiralPocketMode,
  // ZigZagPocketMode,
  // SingleOffsetPocketMode,
  // ZigZagThenSingleOffsetPocketMode,
  CAreaPocketParams params(tool_diameter / 2,
                           0,                     // double Extra_offset
                           tool_diameter * xyPct, // double Stepover,
                           false,                 // bool From_center,
                           SpiralPocketMode, // PocketMode Mode,
                           0); // double Zig_angle)
  outerArea.SplitAndMakePocketToolpath(toolPath, params);
}

void
rect_pocket(std::list<CCurve> &toolPath, double tool_diameter, double x0,
            double y0, double xlen, double ylen, double xyPct) {
  CCurve rect_c;
  makeRect(rect_c, Point(x0, y0), xlen, ylen);
  CArea rect_a;
  rect_a.append(rect_c);

  // SpiralPocketMode,
  // ZigZagPocketMode,
  // SingleOffsetPocketMode,
  // ZigZagThenSingleOffsetPocketMode,

  CAreaPocketParams params(tool_diameter / 2,
                           0,                     // double Extra_offset
                           tool_diameter * xyPct, // double Stepover,
                           false,                 // bool From_center,
                           ZigZagThenSingleOffsetPocketMode,      // PocketMode Mode,
                           22.5);                    // double Zig_angle)
  rect_a.MakePocketToolpath(toolPath, params);
}

int
main(int ac, char **av) {
  Point p(0, 0);

  double dm[] = {1., 0., 0., 12., 0., 1., 0., 0.,
                 0., 0., 1., 0.,  0., 0., 0., 1.};
  geoff_geometry::Matrix m(dm);
  p.Transform(m);
  printf("p(x=%f, y=%f)\n", p.x, p.y);

  std::list<CCurve> toolPath;
  if (1) {
    circular_pocket(toolPath, 0.125/*tool diameter*/,
                    1.5, 1.5, /* cx,cy */
                    1.5, /* outer radius */
                    0.75, /* inner radius */
                    0.45); /* xy overlap */
  } else {
    rect_pocket(toolPath, 0.125, 0.5, 0.5, 1.50, 3.50,
                0.45); // 0., 0.125, 0.45, 0.060);
  }

  GCodeWriter gcode("pocket.nc", 0);
  cut_area(gcode, toolPath, -0.125, 0., 0.125);
  return 0;
}

/*
 * Local Variables:
 * mode: C++
 * mode: font-lock
 * c-basic-offset: 2
 * tab-width: 8
 * compile-command: "make"
 * End:
 */

/* end of /Users/eric/work/github/heeks/libarea/test.cpp */

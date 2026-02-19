/***************************************************
 * file: machining/libarea/node.js/node-area.cpp
 *
 * @file    node-area.cpp
 * @author  Eric L. Hernes
 * @version V1.0
 * @born_on   Wednesday, December 30, 2020
 * @copyright (C) Copyright Eric L. Hernes 2020
 * @copyright (C) Copyright Q, Inc. 2020
 *
 * @brief   An Eric L. Hernes Signature Series C++ module
 *
 */

#include <napi.h>

#include "Area.h"
#include "Curve.h"
#include "Point.h"

#define  MATH_SIGN(x) (((x) > 0) - ((x) < 0))

static PocketMode
pocket_mode(const std::string &mode) {
  PocketMode rv;
  if (mode == "Spiral") rv = PocketMode::Spiral;
  else if (mode == "ZigZag") rv = PocketMode::ZigZag;
  else if (mode == "SingleOffset") rv = PocketMode::SingleOffset;
  else if (mode == "ZigZagThenSingleOffset") rv = PocketMode::ZigZagThenSingleOffset;
  return rv;
}

static bool
makeCircle(CCurve &curve, const Point &center, double radius) {
  curve.append(center + Point(radius, 0));
  curve.append(CVertex(CVertex::vt_ccw_arc, center + Point(-radius, 0), center));
  curve.append(CVertex(CVertex::vt_ccw_arc, center + Point(-radius, 0), center));
  curve.append(CVertex(CVertex::vt_ccw_arc, center + Point(radius, 0), center));
  return true;
}

static void
circle_pocket(std::list<CCurve> &toolPath, double tool_diameter, PocketMode pm, double zz_angle,
              double cx, double cy, double rOuter, double rInner, double xyPct, const Units &u) {
  fprintf(stderr, "%s: (td %f) (pm %d) (zz %f) (cx %f) (cy %f) (ro %f) (ri %f) (xy %f)\n",
          __func__,
          tool_diameter, pm, zz_angle, cx, cy, rOuter, rInner, xyPct);

  CCurve outerCircle, innerCircle;
  CArea innerArea(u), outerArea(u);

  makeCircle(outerCircle, Point(cx, cy), rOuter);
  outerArea.append(outerCircle);
  outerCircle.FitArcs(u);

  makeCircle(innerCircle, Point(cx, cy), rInner);
  innerArea.append(innerCircle);
  innerCircle.FitArcs(u);

  outerArea.Xor(innerArea);  

  CAreaPocketParams params(tool_diameter / 2,
                           0,                     // double Extra_offset
                           tool_diameter * xyPct, // double Stepover,
                           false,                 // bool From_center,
                           pm, // PocketMode Mode,
                           zz_angle); // double Zig_angle)

  outerArea.SplitAndMakePocketToolpath(toolPath, params);
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
rect_pocket(std::list<CCurve> &toolPath, double tool_diameter, PocketMode pm, double zz_angle,
            double x0, double y0, double xlen, double ylen, double xyPct, const Units &u) {
  CCurve rect_c;
  makeRect(rect_c, Point(x0, y0), xlen, ylen);
  CArea rect_a(u);
  rect_a.append(rect_c);

  CAreaPocketParams params(tool_diameter / 2,
                           0,                     // double Extra_offset
                           tool_diameter * xyPct, // double Stepover,
                           false,                 // bool From_center,
                           pm,      // PocketMode Mode,
                           zz_angle);                    // double Zig_angle)
  rect_a.MakePocketToolpath(toolPath, params);
}

static void
polygon_pocket(std::list<CCurve> &toolPath,
               double tool_diameter, PocketMode pm, double zz_angle, double xyPct,
               const CArea &outer_a, const CArea &inner_a) {

  CArea poly_a = outer_a;
  poly_a.Xor(inner_a);

  CAreaPocketParams params(tool_diameter / 2,
                           0,                     // double Extra_offset
                           tool_diameter * xyPct, // double Stepover,
                           false,                 // bool From_center,
                           pm,      // PocketMode Mode,
                           zz_angle);                    // double Zig_angle)

  poly_a.SplitAndMakePocketToolpath(toolPath, params);
}

struct ParameterValidator;

static bool validate_object_properties(const Napi::Value &val, const std::vector<ParameterValidator> &props,
                                       std::string &err);

struct ParameterValidator {
  std::string name;
  std::function<bool(const Napi::Value &val, std::string &)> validator;
  static bool isAnyType(const Napi::Value &val, std::string &) {
    fprintf(stderr, "(" "IsArray" " %d)\n", val.IsArray ());
    fprintf(stderr, "(" "IsArrayBuffer" " %d)\n", val.IsArrayBuffer ());
    fprintf(stderr, "(" "IsBoolean" " %d)\n", val.IsBoolean ());
    fprintf(stderr, "(" "IsBuffer" " %d)\n", val.IsBuffer ());
    fprintf(stderr, "(" "IsDataView" " %d)\n", val.IsDataView ());
#if (NAPI_VERSION > 4)
    fprintf(stderr, "(" "IsDate" " %d)\n", val.IsDate ());
#endif
    fprintf(stderr, "(" "IsEmpty" " %d)\n", val.IsEmpty ());
    fprintf(stderr, "(" "IsExternal" " %d)\n", val.IsExternal ());
    fprintf(stderr, "(" "IsFunction" " %d)\n", val.IsFunction ());
    fprintf(stderr, "(" "IsNull" " %d)\n", val.IsNull ());
    fprintf(stderr, "(" "IsNumber" " %d)\n", val.IsNumber ());
    fprintf(stderr, "(" "IsObject" " %d)\n", val.IsObject ());
    fprintf(stderr, "(" "IsPromise" " %d)\n", val.IsPromise ());
    fprintf(stderr, "(" "IsString" " %d)\n", val.IsString ());
    fprintf(stderr, "(" "IsSymbol" " %d)\n", val.IsSymbol ());
    fprintf(stderr, "(" "IsTypedArray" " %d)\n", val.IsTypedArray ());
    fprintf(stderr, "(" "IsUndefined" " %d)\n", val.IsUndefined ());
    return true;
  };
  static bool isObject(const Napi::Value &val, std::string &) { return val.IsObject(); };
  static bool isBoolean(const Napi::Value &val, std::string &) { return val.IsBoolean(); };
  static bool isString(const Napi::Value &val, std::string &) { return val.IsString(); };
  static bool isNumber(const Napi::Value &val, std::string &) { return val.IsNumber(); };

  static bool isArrayOf(const Napi::Value &val, const std::vector<ParameterValidator> &props,
                        std::string &err) {
    bool rv = val.IsArray();
    if (rv) {
      Napi::Array ar = val.As<Napi::Array>();
      int n = ar.Length();
      for(unsigned i=0; i<n && rv; i++) {
        rv &= validate_object_properties(ar.Get(i), props, err);
        if (!rv) {
          err = "element " + std::to_string(i) + " does not conform";
        }
      }
    }
    return rv;
  }

  static bool isArrayOf(const Napi::Value &val,
                        std::function<bool(const Napi::Value &val, std::string &err)> validator,
                        std::string &err) {
    bool rv = val.IsArray();
    if (rv) {
      Napi::Array ar = val.As<Napi::Array>();
      int n = ar.Length();
      for(unsigned i=0; i<n && rv; i++) {
        rv &= validator(ar.Get(i), err);
        if (!rv) {
          err = "element " + std::to_string(i) + " does not conform";
        }
      }
    }
    return rv;
  }

  static bool isEnum(const Napi::Value &val, const std::vector<std::string> &valid, std::string &err) {
    bool rv = val.IsString();
    if (rv) {
      std::string ss = static_cast<std::string>(val.ToString());
      bool vv = false;
      for(const auto &v : valid) {
        vv |= (ss == v);
      }
      rv = vv;
      if (!rv) {
        err = ss + " is not an enum value";
      }
    }
    return rv;
  };
};

static const std::vector<std::string> skModeValues = {
  "Spiral",
  "ZigZag",
  "SingleOffset",
  "ZigZagThenSingleOffset"
};

static const std::vector<ParameterValidator> skCutProps = {
  { "tool_diameter",  ParameterValidator::isNumber },
  { "mode", [](const Napi::Value &v, std::string &err)->bool { return ParameterValidator::isEnum(v,skModeValues, err); } },
  { "zz_angle", ParameterValidator::isNumber },
  { "units_scale", ParameterValidator::isNumber },
  { "accuracy", ParameterValidator::isNumber }
};

bool
validate_object_properties(const Napi::Value &val, const std::vector<ParameterValidator> &props,
                           std::string &err) {
  err = "";
  bool rv=val.IsObject();
  if (rv) {
    auto ob = val.As<Napi::Object>();
    for(auto p = props.begin(); rv==true && p!=props.end(); ++p) {
      rv = ob.Has(p->name) && p->validator(ob.Get(p->name),err);
      if ((!rv)) {
        if (!ob.Has(p->name)) {
          err = p->name + " is missing";
        } else {
          err = p->name + " failed validation";
        }
      }
    }
  }
  return rv;
}

static const std::vector<ParameterValidator> skCirclePositionProps = {
  { "xc", ParameterValidator::isNumber },
  { "yc", ParameterValidator::isNumber },
  { "rInner", ParameterValidator::isNumber },
  { "rOuter", ParameterValidator::isNumber }
};

static const std::vector<ParameterValidator> skCircleProps = {
  { "finish", ParameterValidator::isBoolean },
  { "xyPct", ParameterValidator::isNumber },
  { "position", [](const Napi::Value &v, std::string &err)->bool {
      return ParameterValidator::isArrayOf(v, skCirclePositionProps, err);
    } }
};

void 
point_to_object(Napi::Object &ob, const CVertex &cv) {
  switch (cv.m_type) {
  case 0: // line or start point
    ob.Set("x", cv.m_p.x);
    ob.Set("y", cv.m_p.y);
    break;
  case -1:  // clockwise arc
    // out.OnArcCW(v.m_p.x, v.m_p.y, v.m_c.y - px, v.m_c.y - py);
    ob.Set("dir", -1);
    ob.Set("cx", cv.m_c.x);
    ob.Set("cy", cv.m_c.y);
    ob.Set("ex", cv.m_p.x);
    ob.Set("ey", cv.m_p.y);
        
    break;
  case 1: // counterclockwise arc
    // out.OnArcCCW(v.m_p.x, v.m_p.y, v.m_c.x - px, v.m_c.y - py);
    ob.Set("dir", 1);
    ob.Set("cx", cv.m_c.x);
    ob.Set("cy", cv.m_c.y);
    ob.Set("ex", cv.m_p.x);
    ob.Set("ey", cv.m_p.y);
    break;
  }
}

static void
mk_return_toolpath(Napi::Array &rv, std::list<CCurve> toolPath) {
//    fprintf(stderr, "outerPath: %lu\n", toolPath.size());
  int cx=0;
  for (const auto c : toolPath) {
    Napi::Array cc = Napi::Array::New(rv.Env(), c.m_vertices.size());
    int cvx=0;
//        fprintf(stderr, "vertices: %lu\n", c.m_vertices.size());
    for (const auto cv : c.m_vertices) {
      Napi::Object ob = Napi::Object::New(rv.Env());
      point_to_object(ob, cv);
      cc.Set(cvx++, ob);
    }
    rv.Set(cx++, cc);
  }
}

static Napi::Array
napi_circle_pocket(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  if(info.Length()!=2) {
    Napi::TypeError::New(env, "Expected 2 Arguments").ThrowAsJavaScriptException();
  }
  std::string err;
  bool st = validate_object_properties(info[0], skCutProps, err);
  if (!st) {
    Napi::TypeError::New(env, "arg[0]: " + err).ThrowAsJavaScriptException();
  }

  if (st) {
    st = validate_object_properties(info[1], skCircleProps, err);
    if (!st) {
      Napi::TypeError::New(env, "arg[1]: " + err).ThrowAsJavaScriptException();            
    }
  }

  Napi::Array rv = Napi::Array::New(env);

  if (st) {
    double tool_diameter = info[0].As<Napi::Object>().Get("tool_diameter").ToNumber().DoubleValue();
    PocketMode pm = pocket_mode(static_cast<std::string>(info[0].As<Napi::Object>().Get("mode").ToString()));
    double zz_angle = info[0].As<Napi::Object>().Get("zz_angle").ToNumber().DoubleValue();
    double units_scale = info[0].As<Napi::Object>().Get("units_scale").ToNumber().DoubleValue();
    double accuracy = info[0].As<Napi::Object>().Get("accuracy").ToNumber().DoubleValue();

    bool finish  = info[1].As<Napi::Object>().Get("finish").ToBoolean().Value();
    double xyPct = info[1].As<Napi::Object>().Get("xyPct").ToNumber().DoubleValue()/100.;

    std::list<CCurve> toolPath;

    Napi::Array ar = info[1].As<Napi::Object>().Get("position").As<Napi::Array>();
    int n = ar.Length();
    Units units(units_scale, accuracy);
    for(unsigned i=0; i<n; i++) {
      Napi::Object ob = ar.Get(i).As<Napi::Object>();
      circle_pocket(toolPath, tool_diameter, pm, zz_angle,
                    ob.Get("xc").ToNumber().DoubleValue(), ob.Get("yc").ToNumber().DoubleValue(),
                    ob.Get("rOuter").ToNumber().DoubleValue(), ob.Get("rInner").ToNumber().DoubleValue(),
                    xyPct, units);
    }
    mk_return_toolpath(rv, toolPath);
  }

  return rv;
}

static Napi::Array
napi_rectangle_pocket(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  Napi::Array rv = Napi::Array::New(env);
  return rv;
}

static const std::vector<ParameterValidator> skPolygonPointProps = {
  { "x", ParameterValidator::isNumber },
  { "y", ParameterValidator::isNumber }
};

static const std::vector<ParameterValidator> skPolygonArcProps = {
  { "cx", ParameterValidator::isNumber }, // center x
  { "cy", ParameterValidator::isNumber }, // center y
  { "ex", ParameterValidator::isNumber }, // ending x
  { "ey", ParameterValidator::isNumber }, // ending y
  { "dir", ParameterValidator::isNumber }, // direction -1=cw, 1=ccw
};

static bool
isPolygon(const Napi::Value &v, std::string &err) {
  return ParameterValidator::isArrayOf(v, [](const Napi::Value &val, std::string &err)->bool {
      std::string e1;
      bool v1 = validate_object_properties(val, skPolygonPointProps, e1);
      std::string e2;
      bool v2 = validate_object_properties(val, skPolygonArcProps, e2);
      bool rv =(v1 || v2);
      if (!rv) {
	err = e1 + " / " + e2;
      }
      return rv;
    }, err);
}
    
static const std::vector<ParameterValidator> skPolygonProps = {
  { "xyPct", ParameterValidator::isNumber },
  { "outerPath", isPolygon },
  { "innerPath", [](const Napi::Value &v, std::string &err)->bool {
      return ParameterValidator::isArrayOf(v, isPolygon, err);
    } }
};

static void
mk_curve_from_array(CCurve &curve, const Napi::Array &ar) {
  int n = ar.Length();
  for(unsigned i=0; i<n; i++) {
    Napi::Object ob = ar.Get(i).As<Napi::Object>();

    if (ob.Has("x")) { // point/line
      double x=ob.Get("x").ToNumber().DoubleValue();
      double y=ob.Get("y").ToNumber().DoubleValue();
      curve.append(Point(x,y));

    } else if(ob.Has("cx")) { // arc
      double cx=ob.Get("cx").ToNumber().DoubleValue();
      double cy=ob.Get("cy").ToNumber().DoubleValue();
      double ex=ob.Get("ex").ToNumber().DoubleValue();
      double ey=ob.Get("ey").ToNumber().DoubleValue();
      int32_t dir=ob.Get("dir").ToNumber().Int32Value();

      CVertex::Type type = (dir<0) ? CVertex::vt_cw_arc : CVertex::vt_ccw_arc;
      curve.append(CVertex(type, Point(ex,ey), Point(cx,cy)));
                
    } else { // something else
    }
  }
  // XXX-ELH: polygon must start with a point for now
  // this closes the object
  if (0) {
    Napi::Object ob = ar.Get(uint32_t(0)).As<Napi::Object>();
    double x=ob.Get("x").ToNumber().DoubleValue();
    double y=ob.Get("y").ToNumber().DoubleValue();
    curve.append(Point(x,y));
  }
}

static Napi::Array
napi_polygon_pocket(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  if(info.Length()!=2) {
    Napi::TypeError::New(env, "Expected 2 Arguments").ThrowAsJavaScriptException();
  }
  std::string err;
  bool st = validate_object_properties(info[0], skCutProps, err);
  if (!st) {
    Napi::TypeError::New(env, "arg[0]: " + err).ThrowAsJavaScriptException();
  }

  if (st) {
    st = validate_object_properties(info[1], skPolygonProps, err);
    if (!st) {
      Napi::TypeError::New(env, "arg[1]: " + err).ThrowAsJavaScriptException();            
    }
  }

  Napi::Array rv = Napi::Array::New(env);
  try {
    if (st) {
      double tool_diameter = info[0].As<Napi::Object>().Get("tool_diameter").ToNumber().DoubleValue();
      PocketMode pm = pocket_mode(static_cast<std::string>(info[0].As<Napi::Object>().Get("mode").ToString()));
      double zz_angle = info[0].As<Napi::Object>().Get("zz_angle").ToNumber().DoubleValue();
      double units_scale = info[0].As<Napi::Object>().Get("units_scale").ToNumber().DoubleValue();
      double accuracy = info[0].As<Napi::Object>().Get("accuracy").ToNumber().DoubleValue();

      double xyPct = info[1].As<Napi::Object>().Get("xyPct").ToNumber().DoubleValue()/100.;
        
      Napi::Array outer = info[1].As<Napi::Object>().Get("outerPath").As<Napi::Array>();
      Units units(units_scale, accuracy);

      CCurve outer_c;
      mk_curve_from_array(outer_c, outer);
      CArea outer_a(units);
      outer_a.append(outer_c);

      Napi::Array inner = info[1].As<Napi::Object>().Get("innerPath").As<Napi::Array>();

      CArea inner_a(units);
      for(unsigned i=0; i<inner.Length(); i++) {
	CCurve i1;
	mk_curve_from_array(i1, inner.Get(i).As<Napi::Array>());
	inner_a.append(i1);
      }

      std::list<CCurve> toolPath;
//      outer_c.UnFitArcs(units);
//      outer_c.FitArcs(units);
//      fprintf(stderr, "%s: (poly_c has %lu vertices)\n", __func__, outer_c.m_vertices.size());
      polygon_pocket(toolPath, tool_diameter, pm, zz_angle, xyPct, outer_a, inner_a);

      mk_return_toolpath(rv, toolPath);
    }
  } catch (const char *msg) {
    Napi::TypeError::New(env, msg).ThrowAsJavaScriptException();
  }
  return rv;
}

static Napi::Object Init(Napi::Env env, Napi::Object exports)  {
  exports.Set("circle_pocket", Napi::Function::New(env, napi_circle_pocket));
  exports.Set("rectangle_pocket", Napi::Function::New(env, napi_rectangle_pocket));
  exports.Set("polygon_pocket", Napi::Function::New(env, napi_polygon_pocket));
  return exports;
}

NODE_API_MODULE(area, Init)

/* end of machining/libarea/node.js/node-area.cpp */

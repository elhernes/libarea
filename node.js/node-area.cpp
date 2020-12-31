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

PocketMode
pocket_mode(const std::string &mode) {
    PocketMode rv;
    if (mode == "SpiralPocketMode") rv = SpiralPocketMode;
    else if (mode == "ZigZagPocketMode") rv = ZigZagPocketMode;
    else if (mode == "SingleOffsetPocketMode") rv = SingleOffsetPocketMode;
    else if (mode == "ZigZagThenSingleOffsetPocketMode") rv = ZigZagThenSingleOffsetPocketMode;
    return rv;
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

static void
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
polygon_pocket(std::list<CCurve> &toolPath, double tool_diameter, double xyPct,
               const std::vector<Point> &points, const Units &u) {
  CCurve poly_c;
  for(const auto p : points) {
      poly_c.append(p);
  }
  poly_c.append(points[0]);

  CArea poly_a(u);
  poly_a.append(poly_c);

//  PocketMode pm = SpiralPocketMode;
//  PocketMode pm = ZigZagPocketMode;
//  PocketMode pm = SingleOffsetPocketMode;
  PocketMode pm = ZigZagThenSingleOffsetPocketMode;
  
  CAreaPocketParams params(tool_diameter / 2,
                           0,                     // double Extra_offset
                           tool_diameter * xyPct, // double Stepover,
                           false,                 // bool From_center,
                           pm,      // PocketMode Mode,
                           0);                    // double Zig_angle)

  poly_a.MakePocketToolpath(toolPath, params);
}

struct ParameterValidator {
    std::string name;
    std::function<bool(const Napi::Value &val)> validator;
    static bool isAnyType(const Napi::Value &val) {
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
    static bool isObject(const Napi::Value &val) { return val.IsObject(); };
    static bool isBoolean(const Napi::Value &val) { return val.IsBoolean(); };
    static bool isString(const Napi::Value &val) { return val.IsString(); };
    static bool isNumber(const Napi::Value &val) { return val.IsNumber(); };
    static bool isEnum(const Napi::Value &val, const std::vector<std::string> &valid) {
        bool rv = val.IsString();
        if (rv) {
            std::string ss = static_cast<std::string>(val.ToString());
            bool vv = false;
            for(const auto &v : valid) {
                vv |= (ss == v);
            }
            rv = vv;
        }
        return rv;
    };
};

static const std::vector<std::string> skModeValues = {
    "SpiralPocketMode",
    "ZigZagPocketMode",
    "SingleOffsetPocketMode",
    "ZigZagThenSingleOffsetPocketMode"
};

static const std::vector<ParameterValidator> skCirclePositionProps = {
    { "xc", ParameterValidator::isNumber },
    { "yc", ParameterValidator::isNumber },
    { "rInner", ParameterValidator::isNumber },
    { "rOuter", ParameterValidator::isNumber }
};

static const std::vector<ParameterValidator> skCutProps = {
    { "tool_diameter",  ParameterValidator::isNumber },
    { "mode", [](const Napi::Value &v)->bool { return ParameterValidator::isEnum(v,skModeValues); } },
    { "zz_angle", ParameterValidator::isNumber }
};

static bool
validate_object_properties(const Napi::Value &val, const std::vector<ParameterValidator> &props) {
    bool rv=val.IsObject();
    if (rv) {
        auto ob = val.As<Napi::Object>();
        for(auto p = props.begin(); rv==true && p!=props.end(); ++p) {
            rv = ob.Has(p->name) && p->validator(ob.Get(p->name));
            if (0) {
                fprintf(stderr, "%s: (has %d) (v %d)\n",
                    p->name.c_str(), ob.Has(p->name), p->validator(ob.Get(p->name)));
            }
        }
    }
    return rv;
}

static const std::vector<ParameterValidator> skCircleProps = {
    { "finish", ParameterValidator::isBoolean },
    { "zCut", ParameterValidator::isNumber },
    { "xyCut", ParameterValidator::isNumber },
    { "zHeight", ParameterValidator::isNumber },
    { "zDepth", ParameterValidator::isNumber },
    { "position", [](const Napi::Value &v)->bool {
            bool rv = v.IsArray();
            if (rv) {
                Napi::Array ar = v.As<Napi::Array>();
                int n = ar.Length();
                for(unsigned i=0; i<n && rv; i++) {
                    rv &= validate_object_properties(ar.Get(i), skCirclePositionProps);
                }
            }
            return rv;
        } }
};

void 
point_to_object(Napi::Object &ob, const CVertex &cv) {
    ob.Set("x", cv.m_p.x);
    ob.Set("y", cv.m_p.y);
}

static void
return_toolpath(Napi::Array &rv, std::list<CCurve> toolPath) {
    fprintf(stderr, "curves: %lu\n", toolPath.size());
    int cx=0;
    for (const auto c : toolPath) {
        Napi::Array cc = Napi::Array::New(rv.Env(), c.m_vertices.size());
        int cvx=0;
        fprintf(stderr, "vertices: %lu\n", c.m_vertices.size());
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

    bool st = validate_object_properties(info[0], skCutProps);
    if (!st) {
        Napi::TypeError::New(env, "arg[0] doesn't conform").ThrowAsJavaScriptException();
    }

    if (st) {
        st = validate_object_properties(info[1], skCircleProps);
        if (!st) {
            Napi::TypeError::New(env, "arg[1] doesn't conform").ThrowAsJavaScriptException();            
        }
    }

    Napi::Array rv = Napi::Array::New(env);

    if (st) {
        double tool_diameter = info[0].As<Napi::Object>().Get("tool_diameter").ToNumber().DoubleValue();
        PocketMode pm = pocket_mode(static_cast<std::string>(info[0].As<Napi::Object>().Get("mode").ToString()));
        double zz_angle = info[0].As<Napi::Object>().Get("zz_angle").ToNumber().DoubleValue();

        bool finish  = info[1].As<Napi::Object>().Get("finish").ToBoolean().Value();
        double zCut = info[1].As<Napi::Object>().Get("zCut").ToNumber().DoubleValue();
        double xyCut = info[1].As<Napi::Object>().Get("xyCut").ToNumber().DoubleValue();
        double zHeight = info[1].As<Napi::Object>().Get("zHeight").ToNumber().DoubleValue();
        double zDepth = info[1].As<Napi::Object>().Get("zDepth").ToNumber().DoubleValue();
        
        std::list<CCurve> toolPath;

        Napi::Array ar = info[1].As<Napi::Object>().Get("position").As<Napi::Array>();
        int n = ar.Length();
        Units units(25.4, 0.001);
        for(unsigned i=0; i<n; i++) {
            Napi::Object ob = ar.Get(i).As<Napi::Object>();
            circle_pocket(toolPath, tool_diameter, pm, zz_angle,
                          ob.Get("xc").ToNumber().DoubleValue(), ob.Get("yc").ToNumber().DoubleValue(),
                          ob.Get("rOuter").ToNumber().DoubleValue(), ob.Get("rInner").ToNumber().DoubleValue(),
                          xyCut, units);
        }
        return_toolpath(rv, toolPath);
    }

    return rv;
}

/*
arg[0] = tool diameter
arg[1] = parameters
{
   finish: t/f
tool_comp: "inside", "outside", ""
  zCut: % of tool diameter
 xyCut: % of tool diameter
 zHeight: 
 zDepth:
 position: [
   {     x: 
         y: 
         xLen: 
         yLen: 
     } ]
}
 */

static void
napi_rectangle_pocket(const Napi::CallbackInfo& info) {
}

static void
napi_polygon_pocket(const Napi::CallbackInfo& info) {
}

static Napi::Object Init(Napi::Env env, Napi::Object exports)  {
    exports.Set("circle_pocket", Napi::Function::New(env, napi_circle_pocket));
    exports.Set("rectangle_pocket", Napi::Function::New(env, napi_rectangle_pocket));
    exports.Set("polygon_pocket", Napi::Function::New(env, napi_polygon_pocket));
    return exports;
}

NODE_API_MODULE(area, Init)

/* end of machining/libarea/node.js/node-area.cpp */

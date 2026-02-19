// Area.h
// Copyright 2011, Dan Heeks
// This program is released under the BSD license. See the file COPYING for details.
// repository now moved to github

#pragma once

#include "Curve.h"

enum class PocketMode
{
	Spiral,
	ZigZag,
	SingleOffset,
	ZigZagThenSingleOffset,
};

struct CAreaPocketParams
{
	double tool_radius;
	double extra_offset;
	double stepover;
	bool from_center;
	PocketMode mode;
	double zig_angle;
	bool only_cut_first_offset;
	CAreaPocketParams(double Tool_radius, double Extra_offset, double Stepover, bool From_center, PocketMode Mode, double Zig_angle)
		: tool_radius(Tool_radius), extra_offset(Extra_offset), stepover(Stepover),
		  from_center(From_center), mode(Mode), zig_angle(Zig_angle), only_cut_first_offset(false)
	{
	}
};

class Units {
public:
    Units(double a) : m_accuracy(a) {}
    double m_accuracy;
};

struct CAreaProcessingContext {
	bool fit_arcs = true;
	bool please_abort = false;
	double processing_done = 0.0;
	double single_area_processing_length = 0.0;
	double after_MakeOffsets_length = 0.0;
	double MakeOffsets_increment = 0.0;
	double split_processing_length = 0.0;
	bool set_processing_length_in_split = false;
};

class CArea
{
public:
    CArea(const Units &u);
    CArea(const CArea &rhs);
    std::list<CCurve> m_curves;
    Units m_units;

	void append(const CCurve& curve);
	void Subtract(const CArea& a2);
	void Intersect(const CArea& a2);
	void Union(const CArea& a2);
	static CArea UniteCurves(std::list<CCurve> &curves, const Units &u);
	void Xor(const CArea& a2);
	void Offset(double inwards_value);
	void Thicken(double value);
	void FitArcs();
	size_t num_curves() const {return m_curves.size();}
	Point NearestPoint(const Point& p)const;
	void GetBox(CBox2D &box) const;
	void Reorder(CAreaProcessingContext *ctx = nullptr);
	void MakePocketToolpath(std::list<CCurve> &toolpath, const CAreaPocketParams &params, CAreaProcessingContext *ctx = nullptr)const;
	void SplitAndMakePocketToolpath(std::list<CCurve> &toolpath, const CAreaPocketParams &params, CAreaProcessingContext *ctx = nullptr)const;
	void MakeOnePocketCurve(std::list<CCurve> &curve_list, const CAreaPocketParams &params, CAreaProcessingContext *ctx = nullptr)const;
	static bool IsBoolean();
	void Split(std::list<CArea> &m_areas, CAreaProcessingContext *ctx = nullptr)const;
	double GetArea(bool always_add = false)const;
	void SpanIntersections(const Span& span, std::list<Point> &pts)const;
	void CurveIntersections(const CCurve& curve, std::list<Point> &pts)const;
	void InsideCurves(const CCurve& curve, std::list<CCurve> &curves_inside)const;
};

enum class OverlapType
{
	Outside,
	Inside,
	Siblings,
	Crossing,
};

OverlapType GetOverlapType(const CCurve& c1, const CCurve& c2);
OverlapType GetOverlapType(const CArea& a1, const CArea& a2);
bool IsInside(const Point& p, const CCurve& c);
bool IsInside(const Point& p, const CArea& a);

// Area.cpp

// Copyright 2011, Dan Heeks
// This program is released under the BSD license. See the file COPYING for details.

#include "Area.h"
#include "AreaOrderer.h"

#include <map>

//static const double PI = 3.1415926535897932;

CArea::CArea(double accuracy) : m_accuracy(accuracy) {

}

CArea::CArea(const CArea &rhs) : m_curves(rhs.m_curves), m_accuracy(rhs.m_accuracy) {
}

void CArea::append(const CCurve& curve)
{
	m_curves.push_back(curve);
}

void CArea::FitArcs(){
	for(auto &curve : m_curves)
	{
		curve.FitArcs(m_accuracy);
	}
}

Point CArea::NearestPoint(const Point& p)const
{
	double best_dist = 0.0;
	Point best_point = Point(0, 0);
	bool first = true;
	for(const auto &curve : m_curves)
	{
		Point near_point = curve.NearestPoint(p, m_accuracy);
		double dist = near_point.dist(p);
		if(first || dist < best_dist)
		{
			best_dist = dist;
			best_point = near_point;
			first = false;
		}
	}
	return best_point;
}

void CArea::GetBox(CBox2D &box) const
{
	for(const auto &curve : m_curves)
	{
		CCurve &nc = const_cast<CCurve&>(curve);
		nc.GetBox(box);
	}
}

void CArea::Reorder(CAreaProcessingContext *ctx)
{
	// curves may have been added with wrong directions
	// test all kurves to see which one are outsides and which are insides and
	// make sure outsides are anti-clockwise and insides are clockwise

	// returns 0, if the curves are OK
	// returns 1, if the curves are overlapping

	CAreaOrderer ao;
	for(auto &curve : m_curves)
	{
		ao.Insert(&curve, m_accuracy);
		if(ctx && ctx->set_processing_length_in_split)
		{
			ctx->processing_done += (ctx->split_processing_length / m_curves.size());
		}
	}

	*this = ao.ResultArea(m_accuracy);
}

class ZigZag
{
public:
	CCurve zig;
	CCurve zag;
	ZigZag(const CCurve& Zig, const CCurve& Zag):zig(Zig), zag(Zag){}
};

struct ZigZagState
{
	double stepover = 0.0;
	std::list<ZigZag> zigzag_list;
	std::list<CCurve> *curve_list = nullptr;
	bool rightward = true;
	double sin_angle = 0.0;
	double cos_angle = 0.0;
	double sin_minus_angle = 0.0;
	double cos_minus_angle = 0.0;
	double accuracy = 0.0;
	std::list< std::list<ZigZag> > reorder_list_list;
};

static Point rotated_point(const Point &p, const ZigZagState &zz)
{
	return Point(p.x * zz.cos_angle - p.y * zz.sin_angle, p.x * zz.sin_angle + p.y * zz.cos_angle);
}

static Point unrotated_point(const Point &p, const ZigZagState &zz)
{
    return Point(p.x * zz.cos_minus_angle - p.y * zz.sin_minus_angle, p.x * zz.sin_minus_angle + p.y * zz.cos_minus_angle);
}

static CVertex rotated_vertex(const CVertex &v, const ZigZagState &zz)
{
	if(v.m_type)
	{
            // XXX: deal w/ CCW vs CW (?)
		return CVertex(v.m_type, rotated_point(v.m_p, zz), rotated_point(v.m_c, zz));
	}
    return CVertex(v.m_type, rotated_point(v.m_p, zz), Point(0, 0));
}

static CVertex unrotated_vertex(const CVertex &v, const ZigZagState &zz)
{
	if(v.m_type)
	{
            // XXX: deal w/ CCW vs CW (?)
		return CVertex(v.m_type, unrotated_point(v.m_p, zz), unrotated_point(v.m_c, zz));
	}
	return CVertex(v.m_type, unrotated_point(v.m_p, zz), Point(0, 0));
}

static void rotate_area(CArea &a, const ZigZagState &zz)
{
	for(auto &curve : a.m_curves)
	{
		for(auto &vt : curve.m_vertices)
		{
			vt = rotated_vertex(vt, zz);
		}
	}
}

static void test_y_point(int i, const Point& p, Point& best_p, bool &found, int &best_index, double y, bool left_not_right, const ZigZagState &zz)
{
	// only consider points at y
	if(fabs(p.y - y) < 2.0 * zz.accuracy)
	{
		if(found)
		{
			// equal high point
			if(left_not_right)
			{
				// use the furthest left point
				if(p.x < best_p.x)
				{
					best_p = p;
					best_index = i;
				}
			}
			else
			{
				// use the furthest right point
				if(p.x > best_p.x)
				{
					best_p = p;
					best_index = i;
				}
			}
		}
		else
		{
			best_p = p;
			best_index = i;
			found = true;
		}
	}
}

static void make_zig_curve(const CCurve& input_curve, double y0, double y, ZigZagState &zz)
{
	CCurve curve(input_curve);

	if(zz.rightward)
	{
		if(curve.IsClockwise())
			curve.Reverse();
	}
	else
	{
		if(!curve.IsClockwise())
			curve.Reverse();
	}

    // find a high point to start looking from
	Point top_left;
	int top_left_index = 0;
	bool top_left_found = false;
	Point top_right;
	int top_right_index = 0;
	bool top_right_found = false;
	Point bottom_left;
	int bottom_left_index = 0;
	bool bottom_left_found = false;

	int i =0;
	for(const auto &vertex : curve.m_vertices)
	{
		test_y_point(i, vertex.m_p, top_right, top_right_found, top_right_index, y, !zz.rightward, zz);
		test_y_point(i, vertex.m_p, top_left, top_left_found, top_left_index, y, zz.rightward, zz);
		test_y_point(i, vertex.m_p, bottom_left, bottom_left_found, bottom_left_index, y0, zz.rightward, zz);
		i++;
	}

	int start_index = 0;
	int end_index = 0;
	int zag_end_index = 0;

	if(bottom_left_found)start_index = bottom_left_index;
	else if(top_left_found)start_index = top_left_index;

	if(top_right_found)
	{
		end_index = top_right_index;
		zag_end_index = top_left_index;
	}
	else
	{
		end_index = bottom_left_index;
		zag_end_index =  bottom_left_index;
	}
	if(end_index <= start_index)end_index += (i-1);
	if(zag_end_index <= start_index)zag_end_index += (i-1);

    CCurve zig, zag;

    bool zig_started = false;
    bool zig_finished = false;
    bool zag_finished = false;

	int v_index = 0;
	for(int i = 0; i < 2; i++)
	{
		// process the curve twice because we don't know where it will start
		if(zag_finished)
			break;
		bool first_vertex = true;
		for(const auto &vertex : curve.m_vertices)
		{
			if(i == 1 && first_vertex)
			{
				first_vertex = false;
				continue;
			}
			first_vertex = false;

			if(zig_finished)
			{
				zag.m_vertices.push_back(unrotated_vertex(vertex, zz));
				if(v_index == zag_end_index)
				{
					zag_finished = true;
					break;
				}
			}
			else if(zig_started)
			{
				zig.m_vertices.push_back(unrotated_vertex(vertex, zz));
				if(v_index == end_index)
				{
					zig_finished = true;
					if(v_index == zag_end_index)
					{
						zag_finished = true;
						break;
					}
					zag.m_vertices.push_back(unrotated_vertex(vertex, zz));
				}
			}
			else
			{
				if(v_index == start_index)
				{
					zig.m_vertices.push_back(unrotated_vertex(vertex, zz));
					zig_started = true;
				}
			}
			v_index++;
		}
	}

    if(zig_finished)
		zz.zigzag_list.push_back(ZigZag(zig, zag));
}

static void make_zig(const CArea &a, double y0, double y, ZigZagState &zz)
{
	for(const auto &curve : a.m_curves)
	{
		make_zig_curve(curve, y0, y, zz);
	}
}

static void add_reorder_zig(ZigZag &zigzag, ZigZagState &zz)
{
    // look in existing lists

	// see if the zag is part of an existing zig
	if(zigzag.zag.m_vertices.size() > 1)
	{
		const Point& zag_e = zigzag.zag.m_vertices.front().m_p;
		bool zag_removed = false;
		for(auto &zigzag_list : zz.reorder_list_list)
		{
			if(zag_removed) break;
			for(const auto &z : zigzag_list)
			{
				if(zag_removed) break;
				for(const auto &v : z.zig.m_vertices)
				{
					if(zag_removed) break;
					if((fabs(zag_e.x - v.m_p.x) < (2.0 * zz.accuracy)) && (fabs(zag_e.y - v.m_p.y) < (2.0 * zz.accuracy)))
					{
						// remove zag from zigzag
						zigzag.zag.m_vertices.clear();
						zag_removed = true;
					}
				}
			}
		}
	}

	// see if the zigzag can join the end of an existing list
	const Point& zig_s = zigzag.zig.m_vertices.front().m_p;
	for(auto &zigzag_list : zz.reorder_list_list)
	{
		const ZigZag& last_zigzag = zigzag_list.back();
        const Point& e = last_zigzag.zig.m_vertices.back().m_p;
        if((fabs(zig_s.x - e.x) < (2.0 * zz.accuracy)) && (fabs(zig_s.y - e.y) < (2.0 * zz.accuracy)))
		{
            zigzag_list.push_back(zigzag);
			return;
		}
	}

    // else add a new list
    std::list<ZigZag> zigzag_list;
    zigzag_list.push_back(zigzag);
    zz.reorder_list_list.push_back(zigzag_list);
}

static void reorder_zigs(ZigZagState &zz)
{
	for(auto &zigzag : zz.zigzag_list)
	{
        add_reorder_zig(zigzag, zz);
	}

	zz.zigzag_list.clear();

	for(auto &zigzag_list : zz.reorder_list_list)
	{
		if(zigzag_list.size() == 0)continue;

		zz.curve_list->push_back(CCurve());
		bool first_zig = true;
		for(auto It = zigzag_list.begin(); It != zigzag_list.end();)
		{
			const ZigZag &zigzag = *It;
			bool first_vertex = true;
			for(const auto &v : zigzag.zig.m_vertices)
			{
				if(first_vertex && !first_zig)
				{
					first_vertex = false;
					continue; // only add the first vertex if doing the first zig
				}
				first_vertex = false;
				zz.curve_list->back().m_vertices.push_back(v);
			}

			It++;
			if(It == zigzag_list.end())
			{
				bool first_zag_vertex = true;
				for(const auto &v : zigzag.zag.m_vertices)
				{
					if(first_zag_vertex)
					{
						first_zag_vertex = false;
						continue; // don't add the first vertex of the zag
					}
					zz.curve_list->back().m_vertices.push_back(v);
				}
			}
			first_zig = false;
		}
	}
	zz.reorder_list_list.clear();
}

static void zigzag(const CArea &input_a, ZigZagState &zz, CAreaProcessingContext *ctx)
{
	if(input_a.m_curves.size() == 0)
	{
		if(ctx) ctx->processing_done += ctx->single_area_processing_length;
		return;
	}

    zz.accuracy = input_a.m_accuracy;

	CArea a(input_a);
    rotate_area(a, zz);

    CBox2D b;
	a.GetBox(b);

    double x0 = b.MinX() - 1.0;
    double x1 = b.MaxX() + 1.0;

    double height = b.MaxY() - b.MinY();
    int num_steps = int(height / zz.stepover + 1);
    double y = b.MinY();
    Point null_point(0, 0);
	zz.rightward = true;

	if(ctx && ctx->please_abort)return;

	double step_percent_increment = (ctx ? 0.8 * ctx->single_area_processing_length / num_steps : 0.0);

	for(int i = 0; i<num_steps; i++)
	{
		double y0 = y;
		y = y + zz.stepover;
		Point p0(x0, y0);
		Point p1(x0, y);
		Point p2(x1, y);
		Point p3(x1, y0);
		CCurve c;
		c.m_vertices.push_back(CVertex(CVertex::vt_line, p0, null_point, 0));
		c.m_vertices.push_back(CVertex(CVertex::vt_line, p1, null_point, 0));
		c.m_vertices.push_back(CVertex(CVertex::vt_line, p2, null_point, 1));
		c.m_vertices.push_back(CVertex(CVertex::vt_line, p3, null_point, 0));
		c.m_vertices.push_back(CVertex(CVertex::vt_line, p0, null_point, 1));
		CArea a2(input_a.m_accuracy);
		a2.m_curves.push_back(c);
		a2.Intersect(a);
		make_zig(a2, y0, y, zz);
		zz.rightward = !zz.rightward;
		if(ctx && ctx->please_abort)return;
		if(ctx) ctx->processing_done += step_percent_increment;
	}

	reorder_zigs(zz);
	if(ctx) ctx->processing_done += 0.2 * ctx->single_area_processing_length;
}

void CArea::SplitAndMakePocketToolpath(std::list<CCurve> &curve_list, const CAreaPocketParams &params, CAreaProcessingContext *ctx)const
{
	if(ctx) ctx->processing_done = 0.0;

	std::list<CArea> areas;
	if(ctx)
	{
		ctx->split_processing_length = 50.0; // jump to 50 percent after split
		ctx->set_processing_length_in_split = true;
	}
	Split(areas, ctx);
	if(ctx)
	{
		ctx->set_processing_length_in_split = false;
		ctx->processing_done = ctx->split_processing_length;
	}

	if(areas.size() == 0)return;

	double single_area_length = 50.0 / areas.size();

	for(auto &ar : areas)
	{
		if(ctx) ctx->single_area_processing_length = single_area_length;
		ar.MakePocketToolpath(curve_list, params, ctx);
	}
}

void CArea::MakePocketToolpath(std::list<CCurve> &curve_list, const CAreaPocketParams &params, CAreaProcessingContext *ctx)const
{
	ZigZagState zz;
	double radians_angle = params.zig_angle * PI / 180;
	zz.sin_angle = sin(-radians_angle);
	zz.cos_angle = cos(-radians_angle);
	zz.sin_minus_angle = sin(radians_angle);
	zz.cos_minus_angle = cos(radians_angle);
	zz.stepover = params.stepover;

	CArea a_offset = *this;
	double current_offset = params.tool_radius + params.extra_offset;

	a_offset.Offset(current_offset);

	if(params.mode == PocketMode::ZigZag || params.mode == PocketMode::ZigZagThenSingleOffset)
	{
		zz.curve_list = &curve_list;
		zigzag(a_offset, zz, ctx);
	}
	else if(params.mode == PocketMode::Spiral)
	{
		std::list<CArea> areas;
		a_offset.Split(areas, ctx);
		if(ctx && ctx->please_abort)return;
		if(areas.size() == 0)
		{
			if(ctx) ctx->processing_done += ctx->single_area_processing_length;
			return;
		}

		if(ctx) ctx->single_area_processing_length /= areas.size();

		for(auto &a2 : areas)
		{
			a2.MakeOnePocketCurve(curve_list, params, ctx);
		}
	}

	if(params.mode == PocketMode::SingleOffset || params.mode == PocketMode::ZigZagThenSingleOffset)
	{
		// add the single offset too
		for(auto &curve : a_offset.m_curves)
		{
			curve_list.push_back(curve);
		}
	}
}

void CArea::Split(std::list<CArea> &areas, CAreaProcessingContext *ctx)const
{
	if(IsBoolean())
	{
		for(const auto &curve : m_curves)
		{
			areas.push_back(CArea(m_accuracy));
			areas.back().m_curves.push_back(curve);
		}
	}
	else
	{
		CArea a = *this;
		a.Reorder(ctx);

		if(ctx && ctx->please_abort)return;

		for(const auto &curve : a.m_curves)
		{
			if(curve.IsClockwise())
			{
				if(areas.size() > 0)
					areas.back().m_curves.push_back(curve);
			}
			else
			{
				areas.push_back(CArea(m_accuracy));
				areas.back().m_curves.push_back(curve);
			}
		}
	}
}

double CArea::GetArea(bool always_add)const
{
	// returns the area of the area
	double area = 0.0;
	for(const auto &curve : m_curves)
	{
		double a = curve.GetArea();
		if(always_add)area += fabs(a);
		else area += a;
	}
	return area;
}

OverlapType GetOverlapType(const CCurve& c1, const CCurve& c2)
{
    CArea a1(0.001);
	a1.m_curves.push_back(c1);
	CArea a2(0.001);
	a2.m_curves.push_back(c2);

	return GetOverlapType(a1, a2);
}

OverlapType GetOverlapType(const CArea& a1, const CArea& a2)
{
	CArea A1(a1);

	A1.Subtract(a2);
	if(A1.m_curves.size() == 0)
	{
		return OverlapType::Inside;
	}

	CArea A2(a2);
	A2.Subtract(a1);
	if(A2.m_curves.size() == 0)
	{
		return OverlapType::Outside;
	}

	A1 = a1;
	A1.Intersect(a2);
	if(A1.m_curves.size() == 0)
	{
		return OverlapType::Siblings;
	}

	return OverlapType::Crossing;
}
#if 0
bool IsInside(const Point& p, const CCurve& c)
{
	CArea a;
	a.m_curves.push_back(c);
	return IsInside(p, a);
}
#endif

bool IsInside(const Point& p, const CArea& a)
{
    CArea a2(a.m_accuracy);
	CCurve c;
	c.m_vertices.push_back(CVertex(Point(p.x - 0.01, p.y - 0.01)));
	c.m_vertices.push_back(CVertex(Point(p.x + 0.01, p.y - 0.01)));
	c.m_vertices.push_back(CVertex(Point(p.x + 0.01, p.y + 0.01)));
	c.m_vertices.push_back(CVertex(Point(p.x - 0.01, p.y + 0.01)));
	c.m_vertices.push_back(CVertex(Point(p.x - 0.01, p.y - 0.01)));
	a2.m_curves.push_back(c);
	a2.Intersect(a);
	if(fabs(a2.GetArea()) < 0.0004)return false;
	return true;
}

void CArea::SpanIntersections(const Span& span, std::list<Point> &pts)const
{
	// this returns all the intersections of this area with the given span, ordered along the span

	// get all points where this area's curves intersect the span
	std::list<Point> pts2;
	for(const auto &c : m_curves)
	{
		c.SpanIntersections(span, pts2);
	}

	// order them along the span
	std::multimap<double, Point> ordered_points;
	for(auto &p : pts2)
	{
		double t;
		if(span.On(p, &t))
		{
			ordered_points.insert(std::make_pair(t, p));
		}
	}

	// add them to the given list of points
	for(auto &pair : ordered_points)
	{
		pts.push_back(pair.second);
	}
}

void CArea::CurveIntersections(const CCurve& curve, std::list<Point> &pts)const
{
	// this returns all the intersections of this area with the given curve, ordered along the curve
	std::list<Span> spans;
	curve.GetSpans(spans);
	for(auto &span : spans)
	{
		std::list<Point> pts2;
		SpanIntersections(span, pts2);
		for(auto &pt : pts2)
		{
			if(pts.size() == 0)
			{
				pts.push_back(pt);
			}
			else
			{
				if(pt != pts.back())pts.push_back(pt);
			}
		}
	}
}

class ThickLine
{
public:
	CArea m_area;
	CCurve m_curve;

    ThickLine(const CCurve& curve, double accuracy) : m_curve(curve), m_area(accuracy)
	{
		m_area.append(curve);
		m_area.Thicken(0.001);
	}
};

void CArea::InsideCurves(const CCurve& curve, std::list<CCurve> &curves_inside)const
{
	//1. find the intersectionpoints between these two curves.
	std::list<Point> pts;
	CurveIntersections(curve, pts);

	//2.seperate curve2 in multiple curves between these intersections.
	std::list<CCurve> separate_curves;
	curve.ExtractSeparateCurves(pts, separate_curves);

	//3. if the midpoint of a seperate curve lies in a1, then we return it.
	for(auto &sc : separate_curves)
	{
		double length = sc.Perim();
		Point mid_point = sc.PerimToPoint(length * 0.5);
		if(IsInside(mid_point, *this))curves_inside.push_back(sc);
	}
}

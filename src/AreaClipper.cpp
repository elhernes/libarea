// AreaClipper.cpp

// implements CArea methods using Angus Johnson's "Clipper"

#include "Area.h"
#include "clipper.hpp"
using namespace ClipperLib;

using TPolygon = Path;
using TPolyPolygon = Paths;

bool CArea::IsBoolean(){ return false; }

//static const double PI = 3.1415926535897932;
static constexpr double Clipper4Factor = 1000000.0;

class DoubleAreaPoint
{
public:
	double X, Y;

	DoubleAreaPoint(double x, double y){X = x; Y = y;}
	DoubleAreaPoint(const IntPoint& p){X = static_cast<double>(p.X) / Clipper4Factor; Y = static_cast<double>(p.Y) / Clipper4Factor;}
	IntPoint int_point(){return IntPoint(static_cast<long64>(X * Clipper4Factor), static_cast<long64>(Y * Clipper4Factor));}
};

static void AddPoint(std::list<DoubleAreaPoint>& pts, const DoubleAreaPoint& p)
{
	pts.push_back(p);
}

static void AddVertex(std::list<DoubleAreaPoint>& pts, const CVertex& vertex, const CVertex* prev_vertex, double accuracy)
{
	if(vertex.m_type == 0 || prev_vertex == nullptr)
	{
		AddPoint(pts, DoubleAreaPoint(vertex.m_p.x, vertex.m_p.y));
	}
	else
	{
		if(vertex.m_p != prev_vertex->m_p)
		{
		double phi,dphi,dx,dy;
		int Segments;
		int i;
		double ang1,ang2,phit;

		dx = prev_vertex->m_p.x - vertex.m_c.x;
		dy = prev_vertex->m_p.y - vertex.m_c.y;

		ang1=atan2(dy,dx);
		if (ang1<0) ang1+=2.0*PI;
		dx = vertex.m_p.x - vertex.m_c.x;
		dy = vertex.m_p.y - vertex.m_c.y;
		ang2=atan2(dy,dx);
		if (ang2<0) ang2+=2.0*PI;

		if (vertex.m_type == -1)
		{ //clockwise
			if (ang2 > ang1)
				phit=2.0*PI-ang2+ ang1;
			else
				phit=ang1-ang2;
		}
		else
		{ //counter_clockwise
			if (ang1 > ang2)
				phit=-(2.0*PI-ang1+ ang2);
			else
				phit=-(ang2-ang1);
		}

		//what is the delta phi to get an accurancy of aber
		double radius = sqrt(dx*dx + dy*dy);
		dphi=2*acos((radius-accuracy)/radius);

		//set the number of segments
		if (phit > 0)
			Segments=static_cast<int>(ceil(phit/dphi));
		else
			Segments=static_cast<int>(ceil(-phit/dphi));

		if (Segments < 1)
			Segments=1;
		if (Segments > 100)
			Segments=100;

		dphi=phit/(Segments);

		double px = prev_vertex->m_p.x;
		double py = prev_vertex->m_p.y;

		for (i=1; i<=Segments; i++)
		{
			dx = px - vertex.m_c.x;
			dy = py - vertex.m_c.y;
			phi=atan2(dy,dx);

			double nx = vertex.m_c.x + radius * cos(phi-dphi);
			double ny = vertex.m_c.y + radius * sin(phi-dphi);

			AddPoint(pts, DoubleAreaPoint(nx, ny));

			px = nx;
			py = ny;
		}
		}
	}
}

static void MakeLoop(std::list<DoubleAreaPoint>& pts, const DoubleAreaPoint &pt0, const DoubleAreaPoint &pt1, const DoubleAreaPoint &pt2, double radius)
{
	Point p0(pt0.X, pt0.Y);
	Point p1(pt1.X, pt1.Y);
	Point p2(pt2.X, pt2.Y);
	Point forward0 = p1 - p0;
	Point right0(forward0.y, -forward0.x);
	right0.normalize();
	Point forward1 = p2 - p1;
	Point right1(forward1.y, -forward1.x);
	right1.normalize();

        CVertex::Type arc_dir = (radius > 0) ? CVertex::vt_ccw_arc : CVertex::vt_cw_arc;

	CVertex v0(CVertex::vt_line, p1 + right0 * radius, Point(0, 0));
	CVertex v1(arc_dir, p1 + right1 * radius, p1);
	CVertex v2(CVertex::vt_line, p2 + right1 * radius, Point(0, 0));

	AddVertex(pts, v1, &v0, 0.01);
	AddVertex(pts, v2, &v1, 0.01);
}

static void OffsetWithLoops(const TPolyPolygon &pp, TPolyPolygon &pp_new, double inwards_value)
{
	Clipper c;

	bool inwards = (inwards_value > 0);
	bool reverse = false;
	double radius = -fabs(inwards_value);

	if(inwards)
	{
		// add a large square on the outside, to be removed later
		TPolygon p;
		p.push_back(DoubleAreaPoint(-10000.0, -10000.0).int_point());
		p.push_back(DoubleAreaPoint(-10000.0, 10000.0).int_point());
		p.push_back(DoubleAreaPoint(10000.0, 10000.0).int_point());
		p.push_back(DoubleAreaPoint(10000.0, -10000.0).int_point());
		c.AddPath(p, ptSubject, true);
	}
	else
	{
		reverse = true;
	}

	std::list<DoubleAreaPoint> pts;
	for(unsigned int i = 0; i < pp.size(); i++)
	{
		const TPolygon& p = pp[i];

		pts.clear();

		if(p.size() > 2)
		{
			if(reverse)
			{
				for(size_t j = p.size()-1; j > 1; j--)MakeLoop(pts, p[j], p[j-1], p[j-2], radius);
				MakeLoop(pts, p[1], p[0], p[p.size()-1], radius);
				MakeLoop(pts, p[0], p[p.size()-1], p[p.size()-2], radius);
			}
			else
			{
				MakeLoop(pts, p[p.size()-2], p[p.size()-1], p[0], radius);
				MakeLoop(pts, p[p.size()-1], p[0], p[1], radius);
				for(unsigned int j = 2; j < p.size(); j++)MakeLoop(pts, p[j-2], p[j-1], p[j], radius);
			}

			TPolygon loopy_polygon;
			loopy_polygon.reserve(pts.size());
			for(auto &pt : pts)
			{
				loopy_polygon.push_back(pt.int_point());
			}
			c.AddPath(loopy_polygon, ptSubject, true);
			pts.clear();
		}
	}

	//c.ForceOrientation(false);
	c.Execute(ctUnion, pp_new, pftNonZero, pftNonZero);

	if(inwards)
	{
		// remove the large square
		if(pp_new.size() > 0)
		{
			pp_new.erase(pp_new.begin());
		}
	}
	else
	{
		// reverse all the resulting polygons
		TPolyPolygon copy = pp_new;
		pp_new.clear();
		pp_new.resize(copy.size());
		for(unsigned int i = 0; i < copy.size(); i++)
		{
			const TPolygon& p = copy[i];
			TPolygon p_new;
			p_new.resize(p.size());
			size_t size_minus_one = p.size() - 1;
			for(unsigned int j = 0; j < p.size(); j++)p_new[j] = p[size_minus_one - j];
			pp_new[i] = p_new;
		}
	}
}

static void MakeObround(std::list<DoubleAreaPoint>& pts, const Point &pt0, const CVertex &vt1, double radius)
{
	Span span(pt0, vt1);
	Point forward0 = span.GetVector(0.0);
	Point forward1 = span.GetVector(1.0);
	Point right0(forward0.y, -forward0.x);
	Point right1(forward1.y, -forward1.x);
	right0.normalize();
	right1.normalize();

	CVertex v0(pt0 + right0 * radius);
	CVertex v1(vt1.m_type, vt1.m_p + right1 * radius, vt1.m_c);
	CVertex v2(CVertex::vt_ccw_arc, vt1.m_p + right1 * -radius, vt1.m_p);
	CVertex v3(reverseArcType(vt1.m_type), pt0 + right0 * -radius, vt1.m_c);
	CVertex v4(CVertex::vt_ccw_arc, pt0 + right0 * radius, pt0);

        double accuracy = 0.01;

	AddVertex(pts, v0, nullptr, accuracy);
	AddVertex(pts, v1, &v0, accuracy);
	AddVertex(pts, v2, &v1, accuracy);
	AddVertex(pts, v3, &v2, accuracy);
	AddVertex(pts, v4, &v3, accuracy);
}

static void OffsetSpansWithObrounds(const CArea& area, TPolyPolygon &pp_new, double radius)
{
	Clipper c;

	std::list<DoubleAreaPoint> pts;
	for(const auto &curve : area.m_curves)
	{
		pts.clear();
		const CVertex* prev_vertex = nullptr;
		for(const auto &vertex : curve.m_vertices)
		{
			if(prev_vertex)
			{
				MakeObround(pts, prev_vertex->m_p, vertex, radius);

				TPolygon loopy_polygon;
				loopy_polygon.reserve(pts.size());
				for(auto &pt : pts)
				{
					loopy_polygon.push_back(pt.int_point());
				}
				c.AddPath(loopy_polygon, ptSubject, true);
				pts.clear();
			}
			prev_vertex = &vertex;
		}
	}

	pp_new.clear();
	c.Execute(ctUnion, pp_new, pftNonZero, pftNonZero);

	// reverse all the resulting polygons
	TPolyPolygon copy = pp_new;
	pp_new.clear();
	pp_new.resize(copy.size());
	for(unsigned int i = 0; i < copy.size(); i++)
	{
		const TPolygon& p = copy[i];
		TPolygon p_new;
		p_new.resize(p.size());
		size_t size_minus_one = p.size() - 1;
		for(unsigned int j = 0; j < p.size(); j++)p_new[j] = p[size_minus_one - j];
		pp_new[i] = p_new;
	}
}

static void MakePolyPoly( const CArea& area, TPolyPolygon &pp, double accuracy, bool reverse = true ){
	pp.clear();

	std::list<DoubleAreaPoint> pts;
	for(const auto &curve : area.m_curves)
	{
		pts.clear();
		const CVertex* prev_vertex = nullptr;
		for(const auto &vertex : curve.m_vertices)
		{
			if(prev_vertex)AddVertex(pts, vertex, prev_vertex, accuracy);
			prev_vertex = &vertex;
		}

		TPolygon p;
		p.resize(pts.size());
		if(reverse)
		{
			size_t i = pts.size() - 1;// clipper wants them the opposite way to CArea
			for(auto &pt : pts)
			{
				p[i] = pt.int_point();
				i--;
			}
		}
		else
		{
			unsigned int i = 0;
			for(auto &pt : pts)
			{
				p[i] = pt.int_point();
				i++;
			}
		}

		pp.push_back(p);
	}
}

static void MakePoly(const CCurve& curve, TPolygon &p, double accuracy)
{
	std::list<DoubleAreaPoint> pts;
	const CVertex* prev_vertex = nullptr;
	for (const auto &vertex : curve.m_vertices)
	{
		if (prev_vertex)AddVertex(pts, vertex, prev_vertex, accuracy);
		prev_vertex = &vertex;
	}

	p.resize(pts.size());
	{
		unsigned int i = 0;
		for (auto &pt : pts)
		{
			p[i] = pt.int_point();
			i++;
		}
	}
}

static void SetFromResult( CCurve& curve, const TPolygon& p, double accuracy, bool reverse = true, bool fit_arcs = true )
{
	for(unsigned int j = 0; j < p.size(); j++)
	{
		const IntPoint &pt = p[j];
		DoubleAreaPoint dp(pt);
		CVertex vertex(CVertex::vt_line, Point(dp.X, dp.Y), Point(0.0, 0.0));
		if(reverse)curve.m_vertices.push_front(vertex);
		else curve.m_vertices.push_back(vertex);
	}
	// make a copy of the first point at the end
	if(reverse)curve.m_vertices.push_front(curve.m_vertices.back());
	else curve.m_vertices.push_back(curve.m_vertices.front());

	if(fit_arcs)curve.FitArcs(accuracy);
}

static void SetFromResult( CArea& area, const TPolyPolygon& pp, double accuracy, bool reverse = true, bool fit_arcs = true )
{
	// delete existing geometry
	area.m_curves.clear();

	for(unsigned int i = 0; i < pp.size(); i++)
	{
		const TPolygon& p = pp[i];

		area.m_curves.push_back(CCurve());
		CCurve &curve = area.m_curves.back();
		SetFromResult(curve, p, accuracy, reverse, fit_arcs);
    }
}

void CArea::Subtract(const CArea& a2)
{
	Clipper c;
	TPolyPolygon pp1, pp2;
	MakePolyPoly(*this, pp1, m_accuracy);
	MakePolyPoly(a2, pp2, m_accuracy);
	c.AddPaths(pp1, ptSubject, true);
	c.AddPaths(pp2, ptClip, true);
	TPolyPolygon solution;
	c.Execute(ctDifference, solution);
	SetFromResult(*this, solution, m_accuracy);
}

void CArea::Intersect(const CArea& a2)
{
	Clipper c;
	TPolyPolygon pp1, pp2;
	MakePolyPoly(*this, pp1, m_accuracy);
	MakePolyPoly(a2, pp2, m_accuracy);
	c.AddPaths(pp1, ptSubject, true);
	c.AddPaths(pp2, ptClip, true);
	TPolyPolygon solution;
	c.Execute(ctIntersection, solution);
	SetFromResult(*this, solution, m_accuracy);
}

void CArea::Union(const CArea& a2)
{
	Clipper c;
	TPolyPolygon pp1, pp2;
	MakePolyPoly(*this, pp1, m_accuracy);
	MakePolyPoly(a2, pp2, m_accuracy);
	c.AddPaths(pp1, ptSubject, true);
	c.AddPaths(pp2, ptClip, true);
	TPolyPolygon solution;
	c.Execute(ctUnion, solution);
	SetFromResult(*this, solution, m_accuracy);
}

// static
CArea CArea::UniteCurves(std::list<CCurve> &curves, double accuracy)
{
	Clipper c;

	TPolyPolygon pp;

	for (auto &curve : curves)
	{
		TPolygon p;
		MakePoly(curve, p, accuracy);
		pp.push_back(p);
	}

	c.AddPaths(pp, ptSubject, true);
	TPolyPolygon solution;
	c.Execute(ctUnion, solution, pftNonZero, pftNonZero);
	CArea area(accuracy);
	SetFromResult(area, solution, accuracy);
	return area;
}

void CArea::Xor(const CArea& a2)
{
	Clipper c;
	TPolyPolygon pp1, pp2;
	MakePolyPoly(*this, pp1, m_accuracy);
	MakePolyPoly(a2, pp2, m_accuracy);
	c.AddPaths(pp1, ptSubject, true);
	c.AddPaths(pp2, ptClip, true);
	TPolyPolygon solution;
	c.Execute(ctXor, solution);
	SetFromResult(*this, solution, m_accuracy);
}

void CArea::Offset(double inwards_value)
{
	TPolyPolygon pp, pp2;
	MakePolyPoly(*this, pp, m_accuracy, false);
	OffsetWithLoops(pp, pp2, inwards_value);
	SetFromResult(*this, pp2, m_accuracy, false);
	this->Reorder();
}

void CArea::Thicken(double value)
{
	TPolyPolygon pp;
	OffsetSpansWithObrounds(*this, pp, value);
	SetFromResult(*this, pp, m_accuracy, false);
	this->Reorder();
}

void UnFitArcs(CCurve &curve, double accuracy)
{
	std::list<DoubleAreaPoint> pts;
	const CVertex* prev_vertex = nullptr;
	for(const auto &vertex : curve.m_vertices)
	{
		AddVertex(pts, vertex, prev_vertex, accuracy);
		prev_vertex = &vertex;
	}

	curve.m_vertices.clear();

	for(auto &pt : pts)
	{
		CVertex vertex(CVertex::vt_line, Point(pt.X, pt.Y), Point(0.0, 0.0));
		curve.m_vertices.push_back(vertex);
	}
}

// AreaOrderer.cpp
// Copyright (c) 2010, Dan Heeks
// This program is released under the BSD license. See the file COPYING for details.

#include "AreaOrderer.h"
#include "Area.h"

CInnerCurves::CInnerCurves(CInnerCurves* pOuter, const CCurve* curve)
	: m_pOuter(pOuter), m_curve(curve), m_unite_area(nullptr)
{
}

CInnerCurves::~CInnerCurves()
{
    if(m_unite_area) delete m_unite_area;
}

const Units &
CInnerCurves::GetUnits()
{
    return m_unite_area->m_units;
}

void CInnerCurves::Insert(const CCurve* pcurve, const Units &u)
{
	std::list<CInnerCurves*> outside_of_these;
	std::list<CInnerCurves*> crossing_these;

	// check all inner curves
	for(auto *c : m_inner_curves)
	{
		switch(GetOverlapType(*pcurve, *(c->m_curve)))
		{
		case OverlapType::Outside:
			outside_of_these.push_back(c);
			break;

		case OverlapType::Inside:
			// insert in this inner curve
                    c->Insert(pcurve, u);
			return;

		case OverlapType::Siblings:
			break;

		case OverlapType::Crossing:
			crossing_these.push_back(c);
			break;
		}
	}

	// add as a new inner
	CInnerCurves* new_item = new CInnerCurves(this, pcurve);
	this->m_inner_curves.insert(new_item);

	for(auto *c : outside_of_these)
	{
		// move items
		c->m_pOuter = new_item;
		new_item->m_inner_curves.insert(c);
		this->m_inner_curves.erase(c);
	}

	for(auto *c : crossing_these)
	{
		// unite these
		new_item->Unite(c, u);
		this->m_inner_curves.erase(c);
	}
}

void CInnerCurves::GetArea(CArea &area, bool outside, bool use_curve)const
{
	if(use_curve && m_curve)
	{
		area.m_curves.push_back(*m_curve);
		outside = !outside;
	}

	std::list<const CInnerCurves*> do_after;

	for(auto *c : m_inner_curves)
	{
		area.m_curves.push_back(*c->m_curve);
		if(!outside)area.m_curves.back().Reverse();

		if(outside)c->GetArea(area, !outside, false);
		else do_after.push_back(c);
	}

	for(auto *c : do_after)
	{
		c->GetArea(area, !outside, false);
	}
}

void CInnerCurves::Unite(const CInnerCurves* c, const Units &u)
{
	// unite all the curves in c, with this one
	CArea* new_area = new CArea(u);
	new_area->m_curves.push_back(*m_curve);
	delete m_unite_area;
	m_unite_area = new_area;

	CArea a2(u);
	c->GetArea(a2);

	m_unite_area->Union(a2);
	m_unite_area->Reorder();
	bool first = true;
	for(auto &curve : m_unite_area->m_curves)
	{
		if(first)
		{
			m_curve = &curve;
			first = false;
		}
		else
		{
			if(curve.IsClockwise())curve.Reverse();
			Insert(&curve, u);
		}
	}
}

CAreaOrderer::CAreaOrderer()
{
	m_top_level = new CInnerCurves(nullptr, nullptr);
}

void CAreaOrderer::Insert(CCurve* pcurve, const Units &u)
{
	// make them all anti-clockwise as they come in
	if(pcurve->IsClockwise())pcurve->Reverse();

	m_top_level->Insert(pcurve, u);
}

CArea CAreaOrderer::ResultArea(const Units &u)const
{
    CArea a(u);

    if(m_top_level) {
        m_top_level->GetArea(a);
    }
    return a;
}

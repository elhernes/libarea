// AreaOrderer.cpp
// Copyright (c) 2010, Dan Heeks
// This program is released under the BSD license. See the file COPYING for details.

#include "AreaOrderer.h"
#include "Area.h"

CInnerCurves::CInnerCurves(CInnerCurves* pOuter, const CCurve* curve)
	: m_pOuter(pOuter), m_curve(curve)
{
}

const Units &
CInnerCurves::GetUnits()
{
    return m_unite_area->m_units;
}

void CInnerCurves::Insert(const CCurve* pcurve, const Units &u)
{
	std::vector<CInnerCurves*> outside_of_these;
	std::vector<CInnerCurves*> crossing_these;

	// check all inner curves
	for(auto &c : m_inner_curves)
	{
		switch(GetOverlapType(*pcurve, *(c->m_curve)))
		{
		case OverlapType::Outside:
			outside_of_these.push_back(c.get());
			break;

		case OverlapType::Inside:
			// insert in this inner curve
                    c->Insert(pcurve, u);
			return;

		case OverlapType::Siblings:
			break;

		case OverlapType::Crossing:
			crossing_these.push_back(c.get());
			break;
		}
	}

	// add as a new inner
	auto new_item_ptr = std::make_unique<CInnerCurves>(this, pcurve);
	CInnerCurves* new_item = new_item_ptr.get();
	m_inner_curves.push_back(std::move(new_item_ptr));

	for(auto *c : outside_of_these)
	{
		// find and move ownership
		for(auto it = m_inner_curves.begin(); it != m_inner_curves.end(); ++it)
		{
			if(it->get() == c)
			{
				c->m_pOuter = new_item;
				new_item->m_inner_curves.push_back(std::move(*it));
				m_inner_curves.erase(it);
				break;
			}
		}
	}

	for(auto *c : crossing_these)
	{
		// unite these, then remove
		new_item->Unite(c, u);
		for(auto it = m_inner_curves.begin(); it != m_inner_curves.end(); ++it)
		{
			if(it->get() == c)
			{
				m_inner_curves.erase(it);
				break;
			}
		}
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

	for(auto &c : m_inner_curves)
	{
		area.m_curves.push_back(*c->m_curve);
		if(!outside)area.m_curves.back().Reverse();

		if(outside)c->GetArea(area, !outside, false);
		else do_after.push_back(c.get());
	}

	for(auto *c : do_after)
	{
		c->GetArea(area, !outside, false);
	}
}

void CInnerCurves::Unite(const CInnerCurves* c, const Units &u)
{
	// unite all the curves in c, with this one
	m_unite_area = std::make_unique<CArea>(u);
	m_unite_area->m_curves.push_back(*m_curve);

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
	m_top_level = std::make_unique<CInnerCurves>(nullptr, nullptr);
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

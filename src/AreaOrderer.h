// AreaOrderer.h
// Copyright (c) 2010, Dan Heeks
// This program is released under the BSD license. See the file COPYING for details.

#pragma once
#include <list>
#include <set>

class CArea;
class CCurve;

class CAreaOrderer;

class CInnerCurves
{
    CInnerCurves* m_pOuter;
    const CCurve* m_curve; // always empty if top level
    std::set<CInnerCurves*> m_inner_curves;
    CArea *m_unite_area; // new curves made by uniting are stored here

public:
    static CAreaOrderer* area_orderer;
    CInnerCurves(CInnerCurves* pOuter, const CCurve* curve);
    ~CInnerCurves();

    void Insert(const CCurve* pcurve, const class Units &u);
    void GetArea(CArea &area, bool outside = true, bool use_curve = true)const;
    void Unite(const CInnerCurves* c, const class Units &u);
    const class Units &GetUnits();
};

class CAreaOrderer
{
public:
    CInnerCurves* m_top_level;

    CAreaOrderer();

    void Insert(CCurve* pcurve, const class Units &u);
    CArea ResultArea(const Units &u)const;
};

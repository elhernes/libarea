// AreaOrderer.h
// Copyright (c) 2010, Dan Heeks
// This program is released under the BSD license. See the file COPYING for details.

#pragma once
#include <list>
#include <vector>
#include <memory>

class CArea;
class CCurve;

class CAreaOrderer;

class CInnerCurves
{
    CInnerCurves* m_pOuter;
    const CCurve* m_curve; // always empty if top level
    std::vector<std::unique_ptr<CInnerCurves>> m_inner_curves;
    std::unique_ptr<CArea> m_unite_area; // new curves made by uniting are stored here

public:
    CInnerCurves(CInnerCurves* pOuter, const CCurve* curve);

    void Insert(const CCurve* pcurve, const class Units &u);
    void GetArea(CArea &area, bool outside = true, bool use_curve = true)const;
    void Unite(const CInnerCurves* c, const class Units &u);
    const class Units &GetUnits();
};

class CAreaOrderer
{
public:
    std::unique_ptr<CInnerCurves> m_top_level;

    CAreaOrderer();

    void Insert(CCurve* pcurve, const class Units &u);
    CArea ResultArea(const Units &u)const;
};

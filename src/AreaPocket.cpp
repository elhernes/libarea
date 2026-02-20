// AreaPocket.cpp
// Copyright 2011, Dan Heeks
// This program is released under the BSD license. See the file COPYING for details.

// implements CArea::MakeOnePocketCurve

#include "Area.h"

#include <map>
#include <memory>
#include <set>

class IslandAndOffset
{
public:
	const CCurve* island;
	CArea offset;
	std::list<CCurve> island_inners;
	std::list<IslandAndOffset*> touching_offsets;

    IslandAndOffset(const CCurve* Island, const CAreaPocketParams &params, double accuracy) : offset(accuracy)
	{
		island = Island;

		offset.m_curves.push_back(*island);
		offset.m_curves.back().Reverse();

		offset.Offset(-params.stepover);


		if(offset.m_curves.size() > 1)
		{
			bool first = true;
			for(auto &c : offset.m_curves)
			{
				if(first) { first = false; continue; }
				island_inners.push_back(c);
				island_inners.back().Reverse();
			}
			offset.m_curves.resize(1);
		}
	}
};

class CurveTree
{
    void MakeOffsets2(double accuracy, std::list<CurveTree*> &to_do_list, std::list<CurveTree*> &islands_added, CAreaProcessingContext *ctx);
    const CAreaPocketParams &m_params;

public:
    Point point_on_parent;
    CCurve curve;
    std::list<std::unique_ptr<CurveTree>> inners;
    std::list<const IslandAndOffset*> offset_islands;
    CurveTree(const CAreaPocketParams &params, const CCurve &c) : m_params(params), curve(c) {
    }

    void MakeOffsets(double accuracy, CAreaProcessingContext *ctx);
};

class GetCurveItem
{
public:
	CurveTree* curve_tree;
	std::list<CVertex>::iterator EndIt;

	GetCurveItem(CurveTree* ct, std::list<CVertex>::iterator EIt):curve_tree(ct), EndIt(EIt){}

    void GetCurve(CCurve& output, double accuracy, std::list<GetCurveItem> &to_do_list, CAreaProcessingContext *ctx);
	CVertex& back(){std::list<CVertex>::iterator It = EndIt; It--; return *It;}
};

// NOTE: GetCurveItem::GetCurve uses iterators for insertion positions, so we keep
// iterator-based loops here rather than converting to range-for.
void GetCurveItem::GetCurve(CCurve& output, double accuracy, std::list<GetCurveItem> &to_do_list, CAreaProcessingContext *ctx)
{
	// walk around the curve adding spans to output until we get to an inner's point_on_parent
	// then add a line from the inner's point_on_parent to inner's start point, then GetCurve from inner

	// add start point
	if(ctx && ctx->please_abort)return;
	output.m_vertices.insert(this->EndIt, CVertex(curve_tree->curve.m_vertices.front()));

	std::list<CurveTree*> inners_to_visit;
	for(auto &inner : curve_tree->inners)
	{
		inners_to_visit.push_back(inner.get());
	}

	const CVertex* prev_vertex = nullptr;

	for(std::list<CVertex>::iterator It = curve_tree->curve.m_vertices.begin(); It != curve_tree->curve.m_vertices.end(); It++)
	{
		const CVertex& vertex = *It;
		if(prev_vertex)
		{
			Span span(prev_vertex->m_p, vertex);

			// order inners on this span
			std::multimap<double, CurveTree*> ordered_inners;
			for(std::list<CurveTree*>::iterator It2 = inners_to_visit.begin(); It2 != inners_to_visit.end();)
			{
				CurveTree *inner = *It2;
				double t;
				if(span.On(inner->point_on_parent, &t))
				{
					ordered_inners.insert(std::make_pair(t, inner));
					It2 = inners_to_visit.erase(It2);
				}
				else
				{
					It2++;
				}
				if(ctx && ctx->please_abort)return;
			}

			if(ctx && ctx->please_abort)return;
			for(auto &pair : ordered_inners)
			{
				CurveTree& inner = *(pair.second);
				if(inner.point_on_parent.dist(back().m_p) > 0.01)
				{
					output.m_vertices.insert(this->EndIt, CVertex(vertex.m_type, inner.point_on_parent, vertex.m_c));
				}
				if(ctx && ctx->please_abort)return;

				// vertex add after GetCurve
				std::list<CVertex>::iterator VIt = output.m_vertices.insert(this->EndIt, CVertex(inner.point_on_parent));

				//inner.GetCurve(output);
				to_do_list.push_back(GetCurveItem(&inner, VIt));
			}

			if(back().m_p != vertex.m_p)output.m_vertices.insert(this->EndIt, vertex);
		}
		prev_vertex = &vertex;
	}

	if(ctx && ctx->please_abort)return;
	for(std::list<CurveTree*>::iterator It2 = inners_to_visit.begin(); It2 != inners_to_visit.end(); It2++)
	{
		CurveTree &inner = *(*It2);
		if(inner.point_on_parent != back().m_p)
		{
			output.m_vertices.insert(this->EndIt, CVertex(inner.point_on_parent));
		}
		if(ctx && ctx->please_abort)return;

		// vertex add after GetCurve
		std::list<CVertex>::iterator VIt = output.m_vertices.insert(this->EndIt, CVertex(inner.point_on_parent));

		//inner.GetCurve(output);
		to_do_list.push_back(GetCurveItem(&inner, VIt));

	}
}

class IslandAndOffsetLink
{
public:
	const IslandAndOffset* island_and_offset;
	CurveTree* add_to;
	IslandAndOffsetLink(const IslandAndOffset* i, CurveTree* a){island_and_offset = i; add_to = a;}
};

static Point GetNearestPoint(CurveTree* curve_tree, std::list<CurveTree*> &islands_added, const CCurve &test_curve, CurveTree** best_curve_tree, double accuracy)
{
	// find nearest point to test_curve, from curve and all the islands in
	double best_dist;
	Point best_point = curve_tree->curve.NearestPoint(test_curve, accuracy,&best_dist);
	*best_curve_tree = curve_tree;
	for(auto *island : islands_added)
	{
		double dist;
		Point p = island->curve.NearestPoint(test_curve, accuracy,&dist);
		if(dist < best_dist)
		{
			*best_curve_tree = island;
			best_point = p;
			best_dist = dist;
		}
	}

	return best_point;
}

void CurveTree::MakeOffsets2(double accuracy, std::list<CurveTree*> &to_do_list, std::list<CurveTree*> &islands_added, CAreaProcessingContext *ctx)
{
	// make offsets

	if(ctx && ctx->please_abort)return;
	CArea smaller(accuracy);
	smaller.m_curves.push_back(curve);
	smaller.Offset(m_params.stepover);

	if(ctx && ctx->please_abort)return;

	// test islands
	for(std::list<const IslandAndOffset*>::iterator It = offset_islands.begin(); It != offset_islands.end();)
	{
		const IslandAndOffset* island_and_offset = *It;

		if(GetOverlapType(island_and_offset->offset, smaller) == OverlapType::Inside)
			It++; // island is still inside
		else
		{
                    inners.push_back(std::make_unique<CurveTree>(m_params, *island_and_offset->island));
			islands_added.push_back(inners.back().get());
			inners.back()->point_on_parent = curve.NearestPoint(*island_and_offset->island, accuracy);
			if(ctx && ctx->please_abort)return;
			Point island_point = island_and_offset->island->NearestPoint(inners.back()->point_on_parent, accuracy);
			if(ctx && ctx->please_abort)return;
			inners.back()->curve.ChangeStart(island_point);
			if(ctx && ctx->please_abort)return;

			// add the island offset's inner curves
			for(const auto &island_inner : island_and_offset->island_inners)
			{
				inners.back()->inners.push_back(std::make_unique<CurveTree>(m_params, island_inner));
				inners.back()->inners.back()->point_on_parent = inners.back()->curve.NearestPoint(island_inner, accuracy);
				if(ctx && ctx->please_abort)return;
				Point island_point = island_inner.NearestPoint(inners.back()->inners.back()->point_on_parent, accuracy);
				if(ctx && ctx->please_abort)return;
				inners.back()->inners.back()->curve.ChangeStart(island_point);
				to_do_list.push_back(inners.back()->inners.back().get()); // do it later, in a while loop
				if(ctx && ctx->please_abort)return;
			}

			smaller.Subtract(island_and_offset->offset);

			std::set<const IslandAndOffset*> added;

			std::list<IslandAndOffsetLink> touching_list;
			for(const auto *touching : island_and_offset->touching_offsets)
			{
				touching_list.push_back(IslandAndOffsetLink(touching, inners.back().get()));
				added.insert(touching);
			}

			while(touching_list.size() > 0)
			{
				IslandAndOffsetLink touching = touching_list.front();
				touching_list.pop_front();
				touching.add_to->inners.push_back(std::make_unique<CurveTree>(m_params, *touching.island_and_offset->island));
				islands_added.push_back(touching.add_to->inners.back().get());
				touching.add_to->inners.back()->point_on_parent = touching.add_to->curve.NearestPoint(*touching.island_and_offset->island, accuracy);
				Point island_point = touching.island_and_offset->island->NearestPoint(touching.add_to->inners.back()->point_on_parent, accuracy);
				touching.add_to->inners.back()->curve.ChangeStart(island_point);
				smaller.Subtract(touching.island_and_offset->offset);

				// add the island offset's inner curves
				for(const auto &island_inner : touching.island_and_offset->island_inners)
				{
					touching.add_to->inners.back()->inners.push_back(std::make_unique<CurveTree>(m_params, island_inner));
					touching.add_to->inners.back()->inners.back()->point_on_parent = touching.add_to->inners.back()->curve.NearestPoint(island_inner, accuracy);
					if(ctx && ctx->please_abort)return;
					Point island_point = island_inner.NearestPoint(touching.add_to->inners.back()->inners.back()->point_on_parent, accuracy);
					if(ctx && ctx->please_abort)return;
					touching.add_to->inners.back()->inners.back()->curve.ChangeStart(island_point);
					to_do_list.push_back(touching.add_to->inners.back()->inners.back().get()); // do it later, in a while loop
					if(ctx && ctx->please_abort)return;
				}

				for(const auto *t2 : touching.island_and_offset->touching_offsets)
				{
					if(added.find(t2)==added.end() && (t2 != island_and_offset))
					{
						touching_list.push_back(IslandAndOffsetLink(t2, touching.add_to->inners.back().get()));
						added.insert(t2);
					}
				}
			}

			if(ctx && ctx->please_abort)return;
			It = offset_islands.erase(It);

			for(const auto *i : added)
			{
				offset_islands.remove(i);
			}

			if(offset_islands.size() == 0)break;
			It = offset_islands.begin();
		}
	}

	if(ctx)
	{
		ctx->processing_done += ctx->MakeOffsets_increment;
		if(ctx->processing_done > ctx->after_MakeOffsets_length)ctx->processing_done = ctx->after_MakeOffsets_length;
	}

	std::list<CArea> separate_areas;
	smaller.Split(separate_areas);
	if(ctx && ctx->please_abort)return;
	for(auto &separate_area : separate_areas)
	{
		CCurve& first_curve = separate_area.m_curves.front();

		CurveTree* nearest_curve_tree = nullptr;
		Point near_point = GetNearestPoint(this, islands_added, first_curve, &nearest_curve_tree, accuracy);

		nearest_curve_tree->inners.push_back(std::make_unique<CurveTree>(m_params, first_curve));

		for(const auto *island_and_offset : offset_islands)
		{
			if(GetOverlapType(island_and_offset->offset, separate_area) == OverlapType::Inside)
				nearest_curve_tree->inners.back()->offset_islands.push_back(island_and_offset);
			if(ctx && ctx->please_abort)return;
		}

		nearest_curve_tree->inners.back()->point_on_parent = near_point;

		if(ctx && ctx->please_abort)return;
		Point first_curve_point = first_curve.NearestPoint(nearest_curve_tree->inners.back()->point_on_parent, accuracy);
		if(ctx && ctx->please_abort)return;
		nearest_curve_tree->inners.back()->curve.ChangeStart(first_curve_point);
		if(ctx && ctx->please_abort)return;
		to_do_list.push_back(nearest_curve_tree->inners.back().get()); // do it later, in a while loop
		if(ctx && ctx->please_abort)return;
	}
}

void CurveTree::MakeOffsets(double accuracy, CAreaProcessingContext *ctx)
{
	std::list<CurveTree*> to_do_list;
	std::list<CurveTree*> islands_added;

	to_do_list.push_back(this);

	while(to_do_list.size() > 0)
	{
		CurveTree* curve_tree = to_do_list.front();
		to_do_list.pop_front();
		curve_tree->MakeOffsets2(accuracy,to_do_list, islands_added, ctx);
	}
}
#if 0 // elh
void recur(std::list<CArea> &arealist, const CArea& a1, const CAreaPocketParams &params, int level)
{
	//if(level > 3)return;

    // this makes arealist by recursively offsetting a1 inwards

	if(a1.m_curves.size() == 0)
		return;

	if(params.from_center)
		arealist.push_front(a1);
	else
		arealist.push_back(a1);

    CArea a_offset = a1;
    a_offset.Offset(params.stepover);

    // split curves into new areas
	if(CArea::IsBoolean())
	{
		for(auto &curve : a_offset.m_curves)
		{
                    CArea a2(a1.m_accuracy);
			a2.m_curves.push_back(curve);
            recur(arealist, a2, params, level + 1);
		}
	}
    else
	{
        // split curves into new areas
		a_offset.Reorder();
        CArea* a2 = nullptr;

		for(auto &curve : a_offset.m_curves)
		{
			if(curve.IsClockwise())
			{
				if(a2 != nullptr)
					a2->m_curves.push_back(curve);
			}
			else
			{
				if(a2 != nullptr)
					recur(arealist, *a2, params, level + 1);
				else
					a2 = new CArea(a1.m_accuracy);
                a2->m_curves.push_back(curve);
			}
		}

		if(a2 != nullptr)
			recur(arealist, *a2, params, level + 1);
	}
}
#endif
void MarkOverlappingOffsetIslands(std::list<IslandAndOffset> &offset_islands)
{
	for(auto It1 = offset_islands.begin(); It1 != offset_islands.end(); It1++)
	{
		auto It2 = It1;
		It2++;
		for(;It2 != offset_islands.end(); It2++)
		{
			IslandAndOffset &o1 = *It1;
			IslandAndOffset &o2 = *It2;

			if(GetOverlapType(o1.offset, o2.offset) == OverlapType::Crossing)
			{
				o1.touching_offsets.push_back(&o2);
				o2.touching_offsets.push_back(&o1);
			}
		}
	}
}

void CArea::MakeOnePocketCurve(std::list<CCurve> &curve_list, const CAreaPocketParams &params, CAreaProcessingContext *ctx)const
{
	if(ctx && ctx->please_abort)return;
#if 0  // simple offsets with feed or rapid joins
	CArea area_for_feed_possible = *this;

	area_for_feed_possible.Offset(-params.tool_radius - 0.01);
	CArea a_offset = *this;

	std::list<CArea> arealist;
	recur(arealist, a_offset, params, 0);

	bool first = true;

	for(auto &area : arealist)
	{
		for(auto &curve : area.m_curves)
		{
			if(!first)
			{
				// try to join these curves with a feed move, if possible and not too long
				CCurve &prev_curve = curve_list.back();
				const Point &prev_p = prev_curve.m_vertices.back().m_p;
				const Point &next_p = curve.m_vertices.front().m_p;

				if(feed_possible(area_for_feed_possible, prev_p, next_p, params.tool_radius))
				{
					// join curves
					prev_curve += curve;
				}
				else
				{
					curve_list.push_back(curve);
				}
			}
			else
			{
				curve_list.push_back(curve);
			}
			first = false;
		}
	}
#else
	if(m_curves.size() == 0)
	{
		if(ctx) ctx->processing_done += ctx->single_area_processing_length;
		return;
	}
	CurveTree top_level(params, m_curves.front());

	std::list<IslandAndOffset> offset_islands;

	bool first = true;
	for(const auto &c : m_curves)
	{
		if(first) { first = false; continue; }
                    IslandAndOffset island_and_offset(&c, params, m_accuracy);
		offset_islands.push_back(island_and_offset);
		top_level.offset_islands.push_back(&(offset_islands.back()));
		if(ctx && ctx->please_abort)return;
	}

	MarkOverlappingOffsetIslands(offset_islands);

	if(ctx) ctx->processing_done += ctx->single_area_processing_length * 0.1;

	if(ctx)
	{
		double MakeOffsets_processing_length = ctx->single_area_processing_length * 0.8;
		ctx->after_MakeOffsets_length = ctx->processing_done + MakeOffsets_processing_length;
		double guess_num_offsets = sqrt(GetArea(true)) * 0.5 / params.stepover;
		ctx->MakeOffsets_increment = MakeOffsets_processing_length / guess_num_offsets;
	}

	top_level.MakeOffsets(m_accuracy, ctx);
	if(ctx && ctx->please_abort)return;
	if(ctx) ctx->processing_done = ctx->after_MakeOffsets_length;

	curve_list.push_back(CCurve());
	CCurve& output = curve_list.back();

	std::list<GetCurveItem> get_curve_to_do_list;
	get_curve_to_do_list.push_back(GetCurveItem(&top_level, output.m_vertices.end()));

	while(get_curve_to_do_list.size() > 0)
	{
		GetCurveItem item = get_curve_to_do_list.front();
		item.GetCurve(output, m_accuracy, get_curve_to_do_list, ctx);
		get_curve_to_do_list.pop_front();
	}

	// unique_ptr handles cleanup when top_level goes out of scope

	if(ctx) ctx->processing_done += ctx->single_area_processing_length * 0.1;
#endif
}

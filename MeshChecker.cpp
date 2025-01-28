#include "MeshChecker.h"

#include <ColourFactory.h>
#include <Constants.h>
#include <DocumentNethers.h>
#include <HalfEdges/HalfEdges.h>
#include <Mesh.h>
#include <Polygon2DList.h>
#include <SegmentList.h>
#include <TriangleOctTree/TriangleOctTree.h>

#include "Plane.h"

static const auto tname = [] (const TrianglePtr& t) -> QString {
    if (t->annotation ().isEmpty ())
    {
        return QStringLiteral ("%1").arg (t->id ());
    }
    return t->annotation ();
};

MeshChecker::MeshChecker (const MeshPtr& mesh) : m_mesh (mesh), m_ttree (mesh->box ())
{
    m_edges = HalfEdges::create (mesh);
    m_ttree.add (m_mesh);
}

MeshChecker::~MeshChecker ()
{
}

bool MeshChecker::check ()
{
    bool ret = true;
    if (m_checks & ShowInfo)
    {
        showInfo ();
    }
    if (m_checks & CheckHoles)
    {
        ret &= checkHoles ();
    }
    if (m_checks & CheckDuplicateTriangles)
    {
        ret &= checkDuplicateTriangles ();
    }
    if (m_checks & CheckShortEdges)
    {
        ret &= checkShortEdges ();
    }

    if (m_checks & CheckReversedTriangles)
    {
        ret &= checkReversedTriangles ();
    }

    if (m_checks & CheckDuplicateVertices)
    {
        ret &= checkDuplicateVertices ();
    }

    if (m_checks & CheckOpenEdges)
    {
        ret &= checkOpenEdges ();
    }

    if (m_checks & CheckHalfEdgeOverlap)
    {
        ret &= checkHalfEdgeOverlap ();
    }

    if (m_checks & CheckTriangleOverlap)
    {
        ret &= checkTriangleOverlap ();
    }
    return ret;
}

bool MeshChecker::checkOpenEdges ()
{
    TriangleList suspects;
    for (const auto& e : qAsConst (m_edges->halfEdgeList ()))
    {
        if (!e->m_pair && !e->testFlag (HalfEdge::Delete))
        {
            suspects.push_back (e->m_triangle);
        }
    }
    auto cnt = suspects.count ();

    auto cmpId = [] (const TrianglePtr& a, const TrianglePtr& b) -> bool {
        return a->id () < b->id ();
    };
    auto cmpArea = [] (const TrianglePtr& a, const TrianglePtr& b) -> bool {
        return a->area2 () > b->area2 ();
    };

    std::sort (suspects.begin (), suspects.end (), cmpId);
    auto dups = std::unique (suspects.begin (), suspects.end ());
    suspects.erase (dups, suspects.end ());
    std::sort (suspects.begin (), suspects.end (), cmpArea);

    QString str = QStringLiteral ("Suspect triangles:");
    int c = 0;
    for (const auto& t : suspects)
    {
        if (c++ % 10 == 0)
        {
            str += QStringLiteral ("\n    ");
        }
        if (t->annotation ().isEmpty ())
        {
            str += QString::number (t->id ()) + " ";
        }
        else
        {
            str += QString::number (t->id ()) + " (" + t->annotation () + ") ";
        }
    }
    verbose (str);
    report (QStringLiteral ("%1 Open edges found").arg (cnt));
    return cnt == 0;
}

bool MeshChecker::checkHoles ()
{
    auto holes = m_edges->holes ();
    if (holes.isEmpty ())
    {
        report (QStringLiteral ("No open edges"));
        return true;
    }

    int idx = -1;
    for (const auto& hole : qAsConst(holes))
    {
        idx++;
        auto area = hole.area ();
        if (area > 0.00000001)
        {
            verbose (QStringLiteral ("Hole: (") + QString::number (area) + "sq)");
            if (m_checks & DrawHoles)
            {
                hole.snapshot (QStringLiteral ("%1.png").arg (idx));
            }
        }
        else
        {
            verbose (QStringLiteral ("Tear:"));
        }
        verbose (hole.toString (2));
    }
    report (QStringLiteral ("%1 Open holes found").arg (holes.count ()));

    return true;
}

bool MeshChecker::checkDuplicateTriangles ()
{
    bool reported = false;
    auto ok = true;
    auto ts = m_mesh->triangles ();
    auto count = ts.count ();
    for (auto i = 0; i < count; i++)
    {
        for (auto j = i + 1; j < count; j++)
        {
            int matches = 0;
            for (int e1 = 0; e1 < 3; e1++)
            {
                for (int e2 = 0; e2 < 3; e2++)
                {
                    if (ts.at (i)->vertexAt (e1) == ts.at (j)->vertexAt (e2) || ts.at (i)->vertexAt (e1)->equal (ts.at (j)->vertexAt (e2), 0.0001))
                    {
                        matches++;
                    }
                }
            }
            if (matches >= 3)
            {
                if (!reported)
                {
                    report (QStringLiteral ("Duplicate triangles found"));
                    reported = true;
                }
                ok = false;
                const auto& t1 = ts.at (i);
                const auto& t2 = ts.at (j);
                if (!t1->annotation ().isEmpty () && !t2->annotation ().isEmpty ())
                {
                    verbose (QStringLiteral ("  T: \"%1\" %2 and T: \"%3\" %4").arg (t1->annotation ()).arg (t1->id ()).arg (t2->annotation ()).arg (t2->id ()));
                }
                else if (!t1->annotation ().isEmpty () && t2->annotation ().isEmpty ())
                {
                    verbose (QStringLiteral ("  T: \"%1\" %2 and T: %3").arg (t1->annotation ()).arg (t1->id ()).arg (t2->id ()));
                }
                else if (t1->annotation ().isEmpty () && !t2->annotation ().isEmpty ())
                {
                    verbose (QStringLiteral ("  T: %1 and T: \"%2\" %3").arg (t1->id ()).arg (t2->annotation ()).arg (t2->id ()));
                }
                else
                {
                    verbose (QStringLiteral ("  T: %1 and T: %2").arg (t1->id ()).arg (t2->id ()));
                }
            }
        }
    }
    if (ok)
    {
        report (QStringLiteral ("No duplicate triangles"));
    }
    return ok;
}

bool MeshChecker::checkShortEdges ()
{
    bool ok = true;
    int foundShortEdge = 0;
    int foundNullEdge = 0;
    double smallest2 = INF;
    for (const auto& edge : qAsConst(m_edges->halfEdgeList ()))
    {
        if (edge->v1 () == edge->v2 ())
        {
            foundNullEdge++;
            ok = false;
            verbose ("Null edge (repeating vertex): " + edge->v1 ()->toString ());
        }
        auto mag2 = edge->magnitude2 ();
        if (mag2 <= Constants::minEdge2)
        {
            foundShortEdge++;
            verbose ("Short edge between vertex: " + edge->v1 ()->toString () + " and " + edge->v2 ()->toString ());
        }
        if (mag2 < smallest2)
        {
            smallest2 = mag2;
        }
    }
    if (smallest2 < INF)
    {
        verbose (QStringLiteral ("Shortest edge value: %1").arg (sqrt (smallest2)));
    }
    if (foundNullEdge)
    {
        report (QStringLiteral ("%1 null edges found (connected by same vertex)").arg (foundNullEdge));
    }
    else
    {
        report (QStringLiteral ("No null edges"));
    }
    if (foundShortEdge)
    {
        report (QStringLiteral ("%1 short edges found").arg (foundShortEdge));
    }
    else
    {
        report (QStringLiteral ("No short edges"));
    }

    return ok;
}

bool MeshChecker::checkReversedTriangles ()
{
    EdgeByVeticesHash hash;
    for (const auto& edge : qAsConst(m_edges->halfEdgeList ()))
    {
        if (!edge->testFlag (HalfEdge::Delete))
        {
            hash.insert (HalfEdgeKey (edge->v1 (), edge->v2 (), false), edge);
        }
    }
    int badCount = 0;
    QList<TrianglePtr> badTriangleCandidates;

    for (const auto& e : qAsConst (m_edges->halfEdgeList ()))
    {
        // any other half edge with same direction
        auto range = hash.equal_range (HalfEdgeKey (e->v1 (), e->v2 (), false));
        auto d = std::distance (range.first, range.second);
        if (d > 1)
        {
            // We have two half edges going the same direction!
            auto it = range.first;
            while (it != range.second)
            {
                badTriangleCandidates << (*it)->m_triangle;
                it++;
            }
        }
    }

    std::sort (badTriangleCandidates.begin (), badTriangleCandidates.end (), [] (const TrianglePtr& a, const TrianglePtr& b) {
        return a->id () < b->id ();
    });

    for (int i = 0; i < badTriangleCandidates.size ();)
    {
        const auto& t = badTriangleCandidates.at (i);
        int count = 0;
        while (i < badTriangleCandidates.size () && badTriangleCandidates.at (i)->id () == t->id ())
        {
            count++;
            i++;
        }
        if (count >= 6)
        {
            badCount++;
            if (!t->annotation ().isEmpty ())
            {
                verbose (QStringLiteral ("Reversed triangle: \"%1\" ID %2 ").arg (t->annotation ()).arg (t->id ()));
            }
            else
            {
                verbose (QStringLiteral ("Reversed triangle: ID %2 ").arg (t->id ()));
            }
        }
    }
    if (badCount)
    {
        report (QStringLiteral ("%1 reversed triangles").arg (badCount));
        return false;
    }
    report (QStringLiteral ("No reversed triangles"));
    return true;
}

void MeshChecker::showInfo ()
{
    auto vcount = m_mesh->vertexList ().count ();
    auto edges = HalfEdges::create (m_mesh);
    auto ecount = edges->halfEdgeList ().count ();

    report (QStringLiteral ("%1 triangles, %2 vertices, %3 half edges (%4 edges)").arg (m_mesh->tcount ()).arg (vcount).arg (ecount).arg (ecount / 2));
}

bool MeshChecker::checkDuplicateVertices ()
{
    auto vs = m_mesh->vertexList ();
    std::sort (vs.begin (), vs.end ());
    auto it = std::adjacent_find (vs.begin (), vs.end ());
    if (it != vs.end ())
    {
        auto ref = *it;
        int badCount = 0;
        while (it != vs.end () && *(*it) == *ref)
        {
            const auto& v = *it;
            if (!v->annotation ().isEmpty ())
            {
                verbose (QStringLiteral ("Duplicate vertex: \"%1\" id: %1").arg (v->annotation ()).arg ((*it)->id ()));
            }
            else
            {
                verbose (QStringLiteral ("Duplicate vertex, id %1").arg ((*it)->id ()));
            }
            it++;
            badCount++;
        }
        if (badCount)
        {
            report (QStringLiteral ("%1 duplicate vertices").arg (badCount));
            return false;
        }
    }
    report (QStringLiteral ("No duplicate vertices"));
    return true;
}

bool MeshChecker::checkHalfEdgeOverlap ()
{
    SegmentList segs;

    for (const auto& t : qAsConst (m_mesh->triangles ()))
    {
        if (!t->testFlag (Triangle::Delete | Triangle::PreDelete))
        {
            for (int e = 0; e < 3; e++)
            {
                auto seg = Segment::create (HalfEdge (t, e));
                segs.push_back (seg);
            }
        }
    }
    // Remove reversed duplicates (half edges)
    for (int i = 0; i < segs.count (); i++)
    {
        for (int j = i + 1; j < segs.count (); j++)
        {
            if (segs.at (i)->start () == segs.at (j)->end () && segs.at (i)->end () == segs.at (j)->start ())
            {
                segs.removeAt (j);
                j--;
            }
        }
    }

    auto vname = [] (const VertexPtr& v) -> QString {
        if (v->annotation ().isEmpty ())
        {
            return QStringLiteral ("%1").arg (v->id ());
        }
        return v->annotation ();
    };

    int badCount = 0;
    for (int i = 0; i < segs.count (); i++)
    {
        auto& seg1 = segs.at (i);  //Segment::create (HalfEdge (t, i));
        for (int j = i + 1; j < segs.count (); j++)
        {
            auto& seg2 = segs.at (j);

            auto res = intersectionOfLines3DMk2 (seg1, seg2);
            switch (res.type)
            {
            case IntersectionOfLines3DMk2Result::None:
            case IntersectionOfLines3DMk2Result::Ends:
                break;
            case IntersectionOfLines3DMk2Result::TJunct:
                badCount++;
                verbose (QStringLiteral ("T junction edges: %1 -> %2 and %3 -> %4").arg (vname (seg1->start ())).arg (vname (seg1->end ())).arg (vname (seg2->start ())).arg (vname (seg2->end ())));
                break;
            case IntersectionOfLines3DMk2Result::Cross:
            case IntersectionOfLines3DMk2Result::InLine:
                 badCount++;
                verbose (QStringLiteral ("Intersecting edges: %1 -> %2 and %3 -> %4").arg (vname (seg1->start ())).arg (vname (seg1->end ())).arg (vname (seg2->start ())).arg (vname (seg2->end ())));

#if 0
                DocumentNethers doc;
                doc.add (seg1);
                doc.add (seg2);
                if (res.intersection1)
                {
                    doc.add (Dot (res.intersection1, ColourFactory::instance ()->colour (1, 0, 1)));
                }
                if (res.intersection2)
                {
                    doc.add (Dot (res.intersection2, ColourFactory::instance ()->colour (1, 0, 1)));
                }
                doc.write (QStringLiteral ("/tmp/3d/intersectSegs.nethers"));
#endif
                break;
            }
        }
    }
    report (QStringLiteral ("%1 overlapping halfEdges.").arg (badCount));
    return badCount == 0;
}

bool MeshChecker::checkTriangleOverlap ()
{
    int badCount = 0;
    for (const auto& t : qAsConst (m_mesh->triangles ()))
    {
        auto box = t->box ();
        auto plane = t->plane ();
        auto candidates = m_ttree.find (box);
        m_mesh->unsetFlag (Triangle::Tagged);

        for (const auto& c : qAsConst(candidates))
        {
            if (!c->testFlag (Triangle::Tagged) && c->box ().intersects (box) && c != t)
            {
                auto p = c->plane ();

                auto segOfIntersection = plane.intersection (p);

                bool intersect = false;

                auto test = [] (const HalfEdge& he1, const HalfEdge& he2)->bool {
                    auto res = intersectionOfLines3DMk2 (he1, he2);
                    return res.type == IntersectionOfLines3DMk2Result::Cross;
                };

                if (!segOfIntersection.isValid ())
                {
                    if (plane.equal (p))
                    {
                        // Coplanar Ts
                        // does the T's half edges intersect the segOfIntersection
                        intersect = test (HalfEdge (c, 0), HalfEdge (t, 0)) ||
                                    test (HalfEdge (c, 0), HalfEdge (t, 1)) ||
                                    test (HalfEdge (c, 0), HalfEdge (t, 2)) ||
                                    test (HalfEdge (c, 1), HalfEdge (t, 0)) ||
                                    test (HalfEdge (c, 1), HalfEdge (t, 1)) ||
                                    test (HalfEdge (c, 1), HalfEdge (t, 2)) ||
                                    test (HalfEdge (c, 2), HalfEdge (t, 0)) ||
                                    test (HalfEdge (c, 2), HalfEdge (t, 1)) ||
                                    test (HalfEdge (c, 2), HalfEdge (t, 2));
                        //intersect = false;
                    }
                }
                else
                {
                    auto test = [] (const Segment& segOfIntersection, const HalfEdge& he)->bool {
                        auto res = intersectionOfLines3DMk2 (segOfIntersection, he);
                        return res.type == IntersectionOfLines3DMk2Result::Cross;
                    };
                    //does the T's half edges intersect the segOfIntersection
                    intersect = test (segOfIntersection, HalfEdge (t, 0)) ||
                                test (segOfIntersection, HalfEdge (t, 1)) ||
                                test (segOfIntersection, HalfEdge (t, 2));
                }

                if (intersect)
                {
                    verbose (QStringLiteral ("Intersecting Ts: %1 and %2").arg (tname (t)).arg (tname (c)));
                    badCount++;
                }
                t->setFlag (Triangle::Tagged);
            }

        }
    }
    if (badCount)
    {
        report (QStringLiteral ("%1 intersecting triangles, out of %2.").arg (badCount).arg (m_mesh->count ()));
        return false;
    }
    report (QStringLiteral ("No intersecting triangles."));
    m_mesh->unsetFlag (Triangle::Tagged);

    return true;
}

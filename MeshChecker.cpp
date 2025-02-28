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

#include <QtConcurrent>

#include "globals.h"

static const auto tname = [] (const TrianglePtr& t) -> QString {
    if (t->annotation ().isEmpty ())
    {
        return QStringLiteral ("%1").arg (t->id ());
    }
    return t->annotation ();
};

MeshChecker::MeshChecker (const MeshPtr& mesh) : m_mesh (mesh)
{
    m_edgesFuture = QtConcurrent::run ([this] {
        return HalfEdges::create (m_mesh);
    });

    m_octtreeFuture = QtConcurrent::run ([this] {
        auto ret = new TriangleOctTree (m_mesh->box ());
        ret->add (m_mesh);
        return ret;
    });
}

MeshChecker::~MeshChecker ()
{
    m_octtreeFuture.waitForFinished ();
    m_edgesFuture.waitForFinished ();
}

bool MeshChecker::check ()
{
    QList<QFuture<QPair<bool, QString>>> futures;

    if (m_checks & ShowInfo)
    {
        auto res = QtConcurrent::run ([this] {
            return showInfo ();
        });
        futures.push_back (res);
    }
    if (m_checks & CheckHoles)
    {
        auto res = QtConcurrent::run ([this] {
            return checkHoles ();
        });
        futures.push_back (res);
    }
    if (m_checks & CheckDuplicateTriangles)
    {
        auto res = QtConcurrent::run ([this] {
            return checkDuplicateTriangles ();
        });
        futures.push_back (res);
    }
    if (m_checks & CheckShortEdges)
    {
        auto res = QtConcurrent::run ([this] {
            return checkShortEdges ();
        });
        futures.push_back (res);
    }

    if (m_checks & CheckReversedTriangles)
    {
        auto res = QtConcurrent::run ([this] {
            return checkReversedTriangles ();
        });
        futures.push_back (res);
    }

    if (m_checks & CheckDuplicateVertices)
    {
        auto res = QtConcurrent::run ([this] {
            return checkDuplicateVertices ();
        });
        futures.push_back (res);
    }

    if (m_checks & CheckOpenEdges)
    {
        auto res = QtConcurrent::run ([this] {
            return checkOpenEdges ();
        });
        futures.push_back (res);
    }

    if (m_checks & CheckHalfEdgeOverlap)
    {
        auto res = QtConcurrent::run ([this] {
            return checkHalfEdgeOverlap ();
        });
        futures.push_back (res);
    }

    if (m_checks & CheckTriangleOverlap)
    {
        auto res = QtConcurrent::run ([this] {
            return checkTriangleOverlap ();
        });
        futures.push_back (res);
    }

    bool ret = true;
    for (const auto& f : futures)
    {
        if (!(m_checks & quiet))
        {
            if (m_checks & verbose)
            {
                out << f.result ().second;
            }
            else
            {
                out << f.result().second.split('\n').constFirst() << '\n';
            }
        }
        ret &= f.result ().first;
    }

    m_octtreeFuture.waitForFinished ();
    m_edgesFuture.waitForFinished ();

    QThread::usleep (20);
    return ret;
}

EdgesPtr MeshChecker::getEdges ()
{
    if (!m_edgesPtr)
    {
        m_edgesPtr = m_edgesFuture.result();
    }
    return m_edgesPtr;
}

QPair<bool, QString> MeshChecker::checkOpenEdges ()
{
    QString ret;

    TriangleList suspects;
    auto edges = getEdges();
    for (const auto& e : qAsConst (edges->halfEdgeList ()))
    {
        if (!e->m_pair && !e->testFlag (HalfEdge::Delete))
        {
            suspects.push_back (e->m_triangle);
        }
    }
    auto cnt = suspects.count ();
    if (cnt == 0)
    {
        ret = QStringLiteral ("  No open edges found\n");
        return { true, ret};
    }
    ret = QStringLiteral ("  %1 Open edges found\n").arg (cnt);

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

    QString str = QStringLiteral ("Suspect triangles:\n");
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
    ret += str;
    return { false, ret};
}

QPair<bool, QString> MeshChecker::checkHoles ()
{
    auto holes = getEdges()->holes ();
    if (holes.isEmpty ())
    {
        return {true, QStringLiteral ("  No holes\n")};
    }
    QString r;

    r = QStringLiteral ("%1 Open holes found").arg (holes.count ());

    //int idx = -1;
    for (const auto& hole : qAsConst(holes))
    {
        //idx++;
        auto area = hole.area ();
        r += QStringLiteral ("Hole: (") + QString::number (area) + "sq)\n";
    }
    return { false, r};
}

QPair<bool, QString> MeshChecker::checkDuplicateTriangles ()
{
    bool reported = false;
    auto ok = true;
    const auto& ts = m_mesh->triangles ();
    auto count = ts.count ();
    const auto& ttree = m_octtreeFuture.result();
    QString r;

    for (auto i = 0; i < count; i++)
    {
        const auto t = ts.at(i);

        auto candidates = ttree->find(t->box ());
        for (const auto& tt : std::as_const(candidates))
        {
            if (t == tt)
            {
                continue;
            }
            int matches = 0;
            for (int e1 = 0; e1 < 3; e1++)
            {
                for (int e2 = 0; e2 < 3; e2++)
                {
                    if (t->vertexAt (e1) == tt->vertexAt (e2) || t->vertexAt (e1)->equal (tt->vertexAt (e2), 0.0001))
                    {
                        matches++;
                    }
                }
            }
            if (matches >= 3)
            {
                if (!reported)
                {
                    r += QStringLiteral ("  Duplicate triangles found\n");
                    reported = true;
                }
                ok = false;

                if (!t->annotation ().isEmpty () && !tt->annotation ().isEmpty ())
                {
                    r += QStringLiteral ("  T: \"%1\" %2 and T: \"%3\" %4\n").arg (t->annotation ()).arg (t->id ()).arg (tt->annotation ()).arg (tt->id ());
                }
                else if (!t->annotation ().isEmpty () && tt->annotation ().isEmpty ())
                {
                    r+= QStringLiteral ("  T: \"%1\" %2 and T: %3\n").arg (t->annotation ()).arg (t->id ()).arg (tt->id ());
                }
                else if (t->annotation ().isEmpty () && !tt->annotation ().isEmpty ())
                {
                    r+= QStringLiteral ("  T: %1 and T: \"%2\" %3\n").arg (t->id ()).arg (tt->annotation ()).arg (tt->id ());
                }
                else
                {
                    r += QStringLiteral ("  T: %1 and T: %2\n").arg (t->id ()).arg (tt->id ());
                }
            }
        }
    }
    if (ok)
    {
        r += QStringLiteral ("  No duplicate triangles\n");
        return {true, r};
    }
    return {false, r};
}

QPair<bool, QString> MeshChecker::checkShortEdges ()
{
    QString ret;

    bool ok = true;
    int foundShortEdge = 0;
    int foundNullEdge = 0;
    double smallest2 = INF;

    for (const auto& edge : qAsConst(getEdges()->halfEdgeList ()))
    {
        if (edge->v1 () == edge->v2 ())
        {
            foundNullEdge++;
            ok = false;
            ret += "  Null edge (repeating vertex): " + edge->v1 ()->toString () + "\n";
        }
        auto mag2 = edge->magnitude2 ();
        if (mag2 <= Constants::minEdge2)
        {
            foundShortEdge++;
            ret += "    Short edge " + QString::number (sqrt (mag2)) + "mm. Between vertex: " + vname (edge->v1()) + " and " + vname (edge->v2 ()) + "\n";
        }
        if (mag2 < smallest2)
        {
            smallest2 = mag2;
        }
    }
    if (smallest2 < INF)
    {
        ret += QStringLiteral ("  Shortest edge value: %1\n").arg (sqrt (smallest2));
    }
    if (foundShortEdge)
    {
        ret.push_front(QStringLiteral ("  %1 short edges found\n").arg (foundShortEdge));
    }
    else
    {
        ret.push_front(QStringLiteral ("  No short edges\n"));
    }
    if (foundNullEdge)
    {
        ret.push_front(QStringLiteral ("  %1 null edges found (connected by same vertex)\n").arg (foundNullEdge));
    }
    else
    {
        ret.push_front(QStringLiteral ("  No null edges\n"));
    }

    return {ok, ret};
}

QPair<bool, QString> MeshChecker::checkReversedTriangles ()
{
    QString ret;

    EdgeByVeticesHash hash;
    for (const auto& edge : qAsConst(getEdges()->halfEdgeList ()))
    {
        if (!edge->testFlag (HalfEdge::Delete))
        {
            hash.insert (HalfEdgeKey (edge->v1 (), edge->v2 (), false), edge);
        }
    }
    int badCount = 0;
    QList<TrianglePtr> badTriangleCandidates;

    for (const auto& e : qAsConst (getEdges()->halfEdgeList ()))
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
                ret += QStringLiteral ("    Reversed triangle: \"%1\" ID %2\n").arg (t->annotation ()).arg (t->id ());
            }
            else
            {
                ret += QStringLiteral ("    Reversed triangle: ID %2\n").arg (t->id ());
            }
        }
    }
    if (badCount)
    {
        ret.push_front(QStringLiteral ("  %1 reversed triangles\n").arg (badCount));
        return {false, ret};
    }
    ret += QStringLiteral ("  No reversed triangles\n");
    return {true, ret};
}

QPair<bool, QString>  MeshChecker::showInfo ()
{
    QString ret;
    auto vcount = m_mesh->vertexList ().count ();
    auto edges = getEdges();
    auto ecount = edges->halfEdgeList ().count ();

    const auto comps = m_mesh->splitComponents (edges);
    if (comps.count () < 2)
    {
        ret = QStringLiteral ("  %1 triangles, %2 vertices, %3 half edges (%4 edges)\n").arg (m_mesh->tcount ()).arg (vcount).arg (ecount).arg (ecount / 2);
    }
    else
    {
        ret = QStringLiteral ("  %5 components, %1 triangles, %2 vertices, %3 half edges (%4 edges)\n").arg (m_mesh->tcount ()).arg (vcount).arg (ecount).arg (ecount / 2).arg(comps.count ());
        int idx = 0;
        for (const auto& comp : comps)
        {
            auto vcount = comp->vertexList ().count ();
            auto edges = HalfEdges::create (comp);
            auto ecount = edges->halfEdgeList ().count ();

            ret += QStringLiteral ("   comp %5: %1 triangles, %2 vertices, %3 half edges (%4 edges)\n").arg (comp->tcount ()).arg (vcount).arg (ecount).arg (ecount / 2).arg(idx++);
        }
    }
    return {true, ret};
}

QPair<bool, QString> MeshChecker::checkDuplicateVertices ()
{
    QString ret;

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
                ret += QStringLiteral ("  Duplicate vertex: \"%1\" id: %1\n").arg (v->annotation ()).arg ((*it)->id ());
            }
            else
            {
                ret += QStringLiteral ("  Duplicate vertex, id %1\n").arg ((*it)->id ());
            }
            it++;
            badCount++;
        }
        if (badCount)
        {
            ret.prepend (QStringLiteral ("  %1 duplicate vertices").arg (badCount));
            return {false, ret};
        }
    }
    ret = QStringLiteral ("  No duplicate vertices\n");
    return {true, ret};
}

auto MeshChecker::vname (const VertexPtr& v) -> QString
{
    if (v->annotation ().isEmpty ())
    {
        return QStringLiteral ("%1").arg (v->id ());
    }
    return v->annotation ();
}

// size_t qHash (const SegmentPtr& seg, size_t seed)
// {
//     return qHash (seg->start()->id (), qHash (seg->end()->id (), seed));
// }

// size_t qHash (const QPair<SegmentPtr, SegmentPtr>& segs, size_t seed)
// {
//     return qHash (segs.first, qHash (segs.second, seed));
// }

// bool operator== (const QPair<SegmentPtr, SegmentPtr>& lhs, const QPair<SegmentPtr, SegmentPtr>&rhs)
// {
//     return lhs
// }

QPair<bool, QString> MeshChecker::checkHalfEdgeOverlap ()
{
    QString ret;

#if 0
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
                ret += QStringLiteral ("    T junction edges: %1 -> %2 and %3 -> %4\n").arg (vname (seg1->start ())).arg (vname (seg1->end ())).arg (vname (seg2->start ())).arg (vname (seg2->end ()));
                break;
            case IntersectionOfLines3DMk2Result::Cross:
            case IntersectionOfLines3DMk2Result::InLine:
                 badCount++;
                ret.push_back(QStringLiteral ("    Intersecting edges: %1 -> %2 and %3 -> %4\n").arg (vname (seg1->start ())).arg (vname (seg1->end ())).arg (vname (seg2->start ())).arg (vname (seg2->end ())));
                break;
            }
        }
    }
#else
    int badCount = 0;
    const auto ttree = *m_octtreeFuture.result ();
    //QSet <QPair<SegmentPtr, SegmentPtr>> reported;
    for (const auto& t : qAsConst (m_mesh->triangles ()))
    {
        if (t->testFlag (Triangle::Delete | Triangle::PreDelete))
        {
            continue;
        }

        const auto candidates = ttree.find (t->box ());
        SegmentPtr segs[3];
        segs[0] = Segment::create (t->vertexAt (0), t->vertexAt (1));
        segs[1] = Segment::create (t->vertexAt (1), t->vertexAt (2));
        segs[2] = Segment::create (t->vertexAt (2), t->vertexAt (0));

        for (const auto& tt : candidates)
        {
            if (t == tt)
            {
                continue;
            }
            for (int ee = 0; ee < 3; ee++)
            {
                auto seg = Segment::create (tt->vertexAt (ee), tt->vertexAt ((ee + 1) % 3));
                for (int e = 0; e < 3; e++)
                {
                    if (seg->start () == segs[e]->end () && seg->end () == segs[e]->start ())
                    {
                        continue;
                    }
                    auto res = intersectionOfLines3DMk2 (segs[e], seg);
                    switch (res.type)
                    {
                    case IntersectionOfLines3DMk2Result::None:
                    case IntersectionOfLines3DMk2Result::Ends:
                        break;
                   case IntersectionOfLines3DMk2Result::InLine: // Unreliable
                    case IntersectionOfLines3DMk2Result::TJunct: // Unreliable
                        // badCount++;
                        // ret += QStringLiteral ("    T junction edges: %1 -> %2 and %3 -> %4\n").arg (vname (segs[e]->start ())).arg (vname (segs[e]->end ())).arg (vname (seg->start ())).arg (vname (seg->end ()));
                        break;
                    case IntersectionOfLines3DMk2Result::Cross:
                        // if (reported.contains({seg, segs[e]}))
                        // {
                        //     continue;
                        // }
                        badCount++;
                        ret.push_back (QStringLiteral ("    Intersecting edges: %1 -> %2 and %3 -> %4\n").arg (vname (segs[ee]->start ())).arg (vname (segs[ee]->end ())).arg (vname (seg->start ())).arg (vname (seg->end ())));
                        //reported.insert ({seg, segs[e]});
                        break;
                    }
                }
            }
        }
    }
#endif
    ret.push_front (QStringLiteral ("  Found %1 overlapping halfEdges (currently each reported twice).\n").arg (badCount / 2));
    return {badCount == 0, ret};
}

QPair<bool, QString> MeshChecker::checkTriangleOverlap ()
{
    QString ret;
    int badCount = 0;
    QList<double> tts;
    QList<double> cts;
    const auto& ttree = *m_octtreeFuture.result();
    for (const auto& t : qAsConst (m_mesh->triangles ()))
    {
        auto box = t->box ();
        auto plane = t->plane ();
        auto candidates = ttree.find (box);
        m_mesh->unsetFlag (Triangle::Tagged);

        for (const auto& c : qAsConst (candidates))
        {
            if (!c->testFlag (Triangle::Tagged) && c->box ().intersects (box) && c != t)
            {
                auto p = c->plane ();

                auto segOfIntersection = plane.intersection (p);

                bool intersect = false;

                auto test = [] (const HalfEdge& he1, const HalfEdge& he2) -> bool {
                    auto res = intersectionOfLines3DMk2 (he1, he2);
                    return res.type == IntersectionOfLines3DMk2Result::Cross;
                };

                if (!segOfIntersection.isValid ())
                {
                    // No intersection of planes - must be parallel or the same plane
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
                    }
                }
                else
                {
                    tts.clear ();
                    cts.clear ();

                    for (int e = 0; e < 3; e++)
                    {
                        auto tres = intersectionOfLines3DMk2 (segOfIntersection, HalfEdge (t, e));
                        if (tres.type == IntersectionOfLines3DMk2Result::Cross)
                        {
                            if (t->contains (tres.intersection1))
                            {
                                tts.push_back (tres.t);
                            }
                        }
                        auto cres = intersectionOfLines3DMk2 (segOfIntersection, HalfEdge (c, e));
                        if (cres.type == IntersectionOfLines3DMk2Result::Cross)
                        {
                            if (t->contains (cres.intersection1))
                            {
                                cts.push_back (cres.t);
                            }
                        }
                    }
                    if (!tts.empty () && !cts.empty ())
                    {
                        std::sort (tts.begin (), tts.end ());
                        std::sort (cts.begin (), cts.end ());

                        intersect = !(cts.constLast () < tts.constFirst () || cts.constFirst () > tts.constLast ());
                    }
                }

                if (intersect)
                {
#if 0
                    auto mesh = Mesh::create ();
                    mesh->add(t);
                    mesh->add(c);
                    DocumentNethers doc (mesh);
                    if (segOfIntersection.isValid ())
                    {
                        doc.add (Segment::create (segOfIntersection));
                    }
                    doc.write("/tmp/3d/overlap.nethers");

                    qDebug ().noquote ().nospace () << t->toCode ("t");
                    qDebug ().noquote ().nospace () << c->toCode ("c");
                    if (segOfIntersection.isValid ())
                    {
                        qDebug ().noquote ().nospace () << segOfIntersection.toCode ("segmentOfIntersection");
                    }
#endif
                    ret.push_back (QStringLiteral ("    Intersecting Ts: %1 and %2\n").arg (tname (t)).arg (tname (c)));
                    badCount++;
                }
                t->setFlag (Triangle::Tagged);
            }
        }
    }
    if (badCount)
    {
        ret.push_front(QStringLiteral ("  Found %1 intersecting triangles, out of %2.\n").arg (badCount).arg (m_mesh->count ()));
    }
    else
    {
        ret += QStringLiteral ("  No intersecting triangles.\n");
    }
    m_mesh->unsetFlag (Triangle::Tagged);

    return { badCount == 0 , ret};
}

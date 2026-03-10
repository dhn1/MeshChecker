#include "MeshChecker.h"

#include <ColourFactory.h>
#include <Constants.h>
#include <DocumentNethers.h>
#include <HalfEdges/HalfEdges.h>
#include <Mesh.h>
#include <Plane.h>
#include <Polygon2DList.h>
#include <PolygonCorefiner.h>
#include <SegmentList.h>
#include <TriangleOctTree/TriangleOctTree.h>

#include <QMutex>
#include <QMutexLocker>
#include <QtConcurrent>

#include "globals.h"

static QMutex flagMutex;  // Used to control access to triangle flags
MeshChecker::CheckList MeshChecker::m_checkList;
bool MeshChecker::m_viewerHyperlinks;

MeshChecker::MeshChecker (const MeshPtr& mesh, const QString& path) : m_mesh (mesh), m_path (path)
{
    m_edgesFuture = QtConcurrent::run ([this] {
        return HalfEdges::create (m_mesh);
    });

    m_octtreeFuture = QtConcurrent::run ([this] {
        if (m_mesh->isEmpty ())
        {
            return new TriangleOctTree (Box (10, 10, 10, 10, 10, 10));
        }
        auto ret = new TriangleOctTree (m_mesh->box ());
        ret->add (m_mesh);
        return ret;
    });
}

QString MeshChecker::fmtName (const TrianglePtr& t)
{
    if (m_viewerHyperlinks)
    {
        return QStringLiteral ("![%1](/t/%2)").arg (t->name (false)).arg (t->id ());
    }
    return t->name (false);
}

QString MeshChecker::fmtName (const VertexPtr& v)
{
    if (m_viewerHyperlinks)
    {
        return QStringLiteral ("![%1](/v/%2)").arg (v->name (false)).arg (v->fileIdx ());
    }
    return v->name (false);
}

// QString MeshChecker::fmtHoles (const Hole& h)
// {
//     if (m_viewerHyperlinks)
//     {
//         return QStringLiteral ("![%1](/h/%2)").arg(h->id (false)).arg(v->name (false));
//     }
//     else
//     {
//         return v->name (false);
//     }
// }

const MeshChecker::CheckList& MeshChecker::checkList ()
{
    if (m_checkList.isEmpty ())
    {
        m_checkList.push_back ({CheckInfo, &MeshChecker::checkInfo});
        m_checkList.push_back ({CheckDuplicateTriangles, &MeshChecker::checkDuplicateTriangles});
        m_checkList.push_back ({CheckOverusedHalfEdges, &MeshChecker::checkOverusedHalfEdges});
        m_checkList.push_back ({CheckOpenEdges, &MeshChecker::checkOpenEdges});
        m_checkList.push_back ({CheckHoles, &MeshChecker::checkHoles});
        m_checkList.push_back ({CheckReversedTriangles, &MeshChecker::checkReversedTriangles});
        m_checkList.push_back ({CheckDuplicateVertices, &MeshChecker::checkDuplicateVertices});
        m_checkList.push_back ({CheckTriangleOverlap, &MeshChecker::checkTriangleOverlap});
        m_checkList.push_back ({CheckDeleted, &MeshChecker::checkDeleted});
        m_checkList.push_back ({CheckVertexLowRefs, &MeshChecker::checkVertexRefs});
        m_checkList.push_back ({CheckUnviableTriangles, &MeshChecker::checkUnviableTriangles});
        m_checkList.push_back ({CheckFlatTriangles, &MeshChecker::checkFlatTriangles});
        m_checkList.push_back ({CheckShortEdges, &MeshChecker::checkShortEdges});
        m_checkList.push_back ({CheckTCount, &MeshChecker::checkTCount});
        m_checkList.push_back ({CheckDuplicateAnnotations, &MeshChecker::checkAnnotations});
        m_checkList.push_back ({CheckComponents, &MeshChecker::checkComponents});
    }
    return m_checkList;
}

MeshChecker::~MeshChecker ()
{
    m_octtreeFuture.waitForFinished ();
    m_edgesFuture.waitForFinished ();
    delete m_octtreeFuture.result ();
}

FileResult MeshChecker::check ()
{
    QLocale const locale;
    FileResult ret;
    ret.m_path = m_path;
    ret.m_pass = true;
    m_summary.clear ();

    for (const auto& c : std::as_const (m_checkList))
    {
        if (m_checks & c.first)
        {
            auto res = (this->*c.second) ();
            ret.m_pass &= res.m_pass || !MeshChecker::failable (c.first);
            ret.m_checkResults.push_back (res);
            if (res.m_badCount >= 0)
            {
                const auto& name = checkName (c.first);
                m_summary += QStringLiteral ("    ") + name + QString (padding - name.length (), QChar ('.')) + QStringLiteral (": ") + locale.toString (res.m_badCount) + QStringLiteral ("\n");
            }
        }
    }

    return ret;
}

HalfEdgesPtr MeshChecker::getEdges ()
{
    if (!m_edgesPtr)
    {
        m_edgesPtr = m_edgesFuture.result ();
    }
    return m_edgesPtr;
}

MeshChecker::CallbackFn MeshChecker::callback () const
{
    return m_callback;
}

void MeshChecker::setCallback (CallbackFn newCallback)
{
    m_callback = newCallback;
}

CheckResult MeshChecker::checkVertexRefs ()
{
    const auto& byV = getEdges ()->edgeByVertex ();
    QString str;
    int badCount = 0;

    auto it = byV.constBegin ();
    if (it != byV.constEnd ())
    {
        while (it != byV.constEnd ())
        {
            int count = 0;
            const VertexPtr v = it.key ();
            while (it != byV.constEnd () && it.key () == v)
            {
                count++;
                it++;
            }
            if (count < 6)
            {
                str += QStringLiteral ("    %1 referenced by half edges only %2 times\n").arg (v->name ()).arg (count);
                badCount++;
            }
        }
    }
    if (badCount)
    {
        str.push_front (QStringLiteral ("  %1 low ref vertices found\n").arg (badCount));

        return {CheckVertexLowRefs, false, str, badCount};
    }
    str.push_front (QStringLiteral ("  No low ref vertices found\n"));
    return {CheckVertexLowRefs, true, str, badCount};
}

CheckResult MeshChecker::checkDeleted ()
{
    QString str;
    int badCount = 0;
    for (const auto& t : *m_mesh)
    {
        if (t->testFlag (Triangle::Delete))
        {
            badCount++;
            str += QStringLiteral ("    %1\n").arg (fmtName (t));
        }
    }
    if (badCount)
    {
        str.push_front (QStringLiteral ("  %1 deleted triangles found\n").arg (badCount));

        return {CheckDeleted, false, str, badCount};
    }
    str.push_front (QStringLiteral ("  No deleted triangles found\n"));
    return {CheckDeleted, true, str, badCount};
}

QString MeshChecker::summary () const
{
    return m_summary;
}

CheckResult MeshChecker::checkOpenEdges ()
{
    QString str;

    auto edges = getEdges ();
    int badCount = 0;

    for (const auto& e : edges->openEdges ())
    {
        if (!e->testFlag (HalfEdge::Delete))
        {
            str += QStringLiteral ("    T: %1, Edge: %2 -> %3\n").arg (fmtName (e->triangle())).arg (fmtName(e->v1 ())).arg (fmtName (e->v2 ()));
            badCount++;
        }
    }
    if (badCount == 0)
    {
        str.push_front (QStringLiteral ("  No open edges found\n"));
        return {CheckOpenEdges, true, str, badCount};
    }
    str.push_front (QStringLiteral ("  %1 Open edges found\n").arg (badCount));
    return {CheckOpenEdges, badCount == 0, str, badCount};
}

CheckResult MeshChecker::checkHoles ()
{
    auto holes = getEdges ()->holes ();
    if (holes.isEmpty ())
    {
        return {CheckHoles, true, QStringLiteral ("  No holes\n"), 0};
    }
    QString r;

    r = QStringLiteral ("  %1 Open holes found\n").arg (holes.count ());

    //int idx = -1;
    for (const auto& hole : qAsConst (holes))
    {
        //idx++;
        auto area = hole->area ();
        r += QStringLiteral ("    Hole: (") + QString::number (area) + QStringLiteral ("sq)\n");
    }
    return {CheckHoles, holes.isEmpty (), r, (int)holes.count ()};
}

class TriangleKey
{
public:
    TriangleKey (const TrianglePtr& t)
    {
        m_vs << t->v1 () << t->v2 () << t->v3 ();
        std::sort (m_vs.begin (), m_vs.end ());
    }
    bool operator== (const TriangleKey& rhs) const { return m_vs.at (0) == rhs.m_vs.at (0) && m_vs.at (1) == rhs.m_vs.at (1) && m_vs.at (2) == rhs.m_vs.at (2); }
    size_t hash () const { return qHashMulti (0, m_vs.at (0), m_vs.at (1), m_vs.at (2)); }

private:
    QList<VertexPtr> m_vs;
};

static size_t qHash (const TriangleKey& tk)
{
    return tk.hash ();
}

CheckResult MeshChecker::checkDuplicateTriangles ()
{
    auto count = m_mesh->count ();
    const auto& ttree = m_octtreeFuture.result ();
    QString r;
    int badCount = 0;

    QHash<TriangleKey, int> thash;

    for (auto i = 0; i < count; i++)
    {
        const auto t = m_mesh->at (i);

        auto candidates = ttree->find (t->box ());
        for (const auto& tt : std::as_const (candidates))
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
                if (!thash.contains (t))
                {
                    r += QStringLiteral ("    T: %1 and T: %2\n").arg (fmtName(t), fmtName (tt));
                    badCount++;
                    thash.insert (TriangleKey (t), 0);
                }
            }
        }
    }
    if (!badCount)
    {
        r += QStringLiteral ("  No duplicate triangles\n");
        return {CheckDuplicateTriangles, true, r, badCount};
    }
    r.push_front (QStringLiteral ("  %1 duplicate triangles\n").arg (badCount));
    return {CheckDuplicateTriangles, false, r, badCount};
}

CheckResult MeshChecker::checkShortEdges ()
{
    QString ret;

    bool ok = true;
    int foundShortEdge = 0;
    int foundNullEdge = 0;
    double smallest2 = INF;

    for (const auto& t : *m_mesh)
    {
        for (int ee = 0; ee < 3; ee++)
        {
            const auto& edge = t->halfEdge (ee);
            if (edge->testFlag (HalfEdge::Delete))
            {
                continue;
            }

            if (edge->v1 () == edge->v2 ())
            {
                foundNullEdge++;
                ok = false;
                ret += QStringLiteral ("    Null edge (repeating vertex): ") + edge->v1 ()->toString () + QStringLiteral ("\n");
            }
            auto mag2 = edge->magnitude2 ();
            if (mag2 <= Constants::minEdge2)
            {
                foundShortEdge++;
                if (edge->pair ())
                {
                    ret += QStringLiteral ("    Short edge ") + QString::number (sqrt (mag2)) + QStringLiteral ("mm. Between vertex: ") + fmtName (edge->v1 ()) + QStringLiteral (" and ") + fmtName (edge->v2 ()) + QStringLiteral (". Ts ") +
                           fmtName (edge->triangle ()) + QStringLiteral (" and ") + fmtName (edge->pair ()->triangle ()) + QStringLiteral (" \n");
                }
                else
                {
                    ret += QStringLiteral ("    Short edge ") + QString::number (sqrt (mag2)) + QStringLiteral ("mm. Between vertex: ") + fmtName (edge->v1 ()) + QStringLiteral (" and ") + fmtName (edge->v2 ()) + QStringLiteral (". Ts ") +
                           fmtName (edge->triangle ()) + QByteArrayLiteral (" and None\n");
                }
            }
            if (mag2 < smallest2)
            {
                smallest2 = mag2;
            }
        }
    }

    if (smallest2 < INF)
    {
        ret += QStringLiteral ("  Shortest edge value: %1\n").arg (sqrt (smallest2));
    }
    if (foundShortEdge)
    {
        ret.push_front (QStringLiteral ("  %1 short half edges found\n").arg (foundShortEdge));
    }
    else
    {
        ret.push_front (QStringLiteral ("  No short edges\n"));
    }
    if (foundNullEdge)
    {
        ret.push_front (QStringLiteral ("  %1 null edges found (connected by same vertex)\n").arg (foundNullEdge));
    }
    else
    {
        ret.push_front (QStringLiteral ("  No null edges\n"));
    }
    return {CheckShortEdges, ok, ret, -1};
}

CheckResult MeshChecker::checkReversedTriangles ()
{
    QString ret;

    QList<TrianglePtr> reversedTriangles;
    const auto hash = getEdges ()->edgeByEdge ();

    for (const auto& t : *m_mesh)
    {
        for (int ee = 0; ee < 3; ee++)
        {
            auto const & hedge = t->halfEdge (ee);

            if (hedge->testFlag (HalfEdge::Delete))
            {
                continue;
            }

            auto values = hash.values ({hedge});
            //Q_ASSERT (values.count () == 1 || values.count() == 2);

            if (values.count () == 2)
            {
                if (values.at (0)->v1 () == values.at (1)->v1 ())
                {
                    reversedTriangles << hedge->triangle ();
                }
            }
        }
    }

    std::sort (reversedTriangles.begin (), reversedTriangles.end (), [] (const TrianglePtr& a, const TrianglePtr& b) {
        return a->id () < b->id ();
    });

    int badCount = 0;
    for (int i = 0; i < reversedTriangles.size ();)
    {
        const auto& t = reversedTriangles.at (i);
        auto count = reversedTriangles.count (t);
        if (count == 3)
        {
            ret += QStringLiteral ("    triangle: %1\n").arg (fmtName (t));
            badCount++;
        }
        i += count;
    }
    if (badCount)
    {
        ret.push_front (QStringLiteral ("  %1 reversed triangles\n").arg (badCount));
        return {CheckReversedTriangles, false, ret, badCount};
    }
    ret += QStringLiteral ("  No reversed triangles\n");
    return {CheckReversedTriangles, true, ret, badCount};
}

CheckResult MeshChecker::checkOverusedHalfEdges ()
{
    QString ret;
    int badCount = 0;
    const auto& edgeByEdge = getEdges ()->edgeByEdge ();

    auto keys = edgeByEdge.keys ();
    std::sort (keys.begin (), keys.end ());
    auto it = std::unique (keys.begin (), keys.end ());
    keys.erase (it, keys.end ());

    for (const auto& key : std::as_const (keys))
    {
        const auto values = edgeByEdge.values (key);
        if (values.count () > 2)
        {
            badCount++;
            const auto& e = values.constFirst ();
            ret.push_back (QStringLiteral ("    edge: %1 -> %2\n").arg (fmtName (e->v1 ()), fmtName(e->v2 ())));
            for (const auto& match : values)
            {
                ret.push_back (QStringLiteral ("      Used by T: %1\n").arg (fmtName(match->triangle ())));
            }
        }
    }

    if (badCount)
    {
        ret.push_front (QStringLiteral ("  %1 overused half edges\n").arg (badCount));
        return {CheckOverusedHalfEdges, false, ret, badCount};
    }
    ret += QStringLiteral ("  No overused half edges\n");
    return {CheckOverusedHalfEdges, true, ret, badCount};
}

CheckResult MeshChecker::checkInfo ()
{
    const QLocale locale;
    QString ret;
    auto vcount = m_mesh->vertexList ().count ();
    auto edges = getEdges ();
    auto ecount = m_mesh->halfEdgeList ().count ();

    const QFileInfo f (m_path);
    ret += QStringLiteral ("  Info\n    Modified: %1\n").arg (f.lastModified ().toString ());

    const auto comps = m_mesh->splitComponents (edges);
    if (comps.count () < 2)
    {
        ret += QStringLiteral ("    %1 triangles, %2 vertices, %3 half edges\n").arg (locale.toString (m_mesh->tcount ())).arg (locale.toString (vcount)).arg (locale.toString (ecount));
    }
    else
    {
        ret += QStringLiteral ("    %4 components, %1 triangles, %2 vertices, %3 half edges\n").arg (locale.toString (m_mesh->tcount ())).arg (locale.toString (vcount)).arg (locale.toString (ecount)).arg (comps.count ());

        int idx = 0;
        for (const auto& comp : comps)
        {
            auto vcount = comp->vertexList ().count ();
            auto edges = HalfEdges::create (comp);
            auto ecount = comp->halfEdgeList ().count ();

            ret += QStringLiteral ("      comp %4: %1 triangles, %2 vertices, %3 half edges\n").arg (locale.toString (comp->tcount ())).arg (locale.toString (vcount)).arg (locale.toString (ecount)).arg (idx++);

            if (comp->count () < 10)
            {
                for (const auto& t : *comp)
                {
                    ret += QStringLiteral ("        T: %1\n").arg (t->name ());
                }
            }
        }
    }
    return {CheckInfo, true, ret, -1};
}

CheckResult MeshChecker::checkDuplicateVertices ()
{
    QString ret;
    int badCount = 0;
    OctTree tree (m_mesh->box ());
    auto vs = m_mesh->vertexList ();
    for (const auto& v : std::as_const (vs))
    {
        auto res = tree.findOrAdd (v);
        if (res != v)
        {
            badCount++;
            ret += QStringLiteral ("    %1 and %2\n").arg (fmtName (v)).arg (fmtName (res));
        }
    }
    if (badCount)
    {
        ret.prepend (QStringLiteral ("  %1 duplicate vertices\n").arg (badCount));
        return {CheckDuplicateVertices, false, ret, badCount};
    }
    return {CheckDuplicateVertices, true, QStringLiteral ("  No duplicate vertices\n"), badCount};
}

CheckResult MeshChecker::checkTriangleOverlap ()
{
    QString ret;
    QList<double> tts;
    QList<double> cts;
    const auto& ttree = *m_octtreeFuture.result ();

    struct OverlapPair
    {
        TrianglePtr first;
        TrianglePtr second;
        double area {};
    };
    QList<OverlapPair> overlaps;

    for (const auto& t : qAsConst (*m_mesh))
    {
        auto plane = t->plane ();
        if (plane.isValid ())
        {
            auto box = t->box ();
            auto candidates = ttree.find (box);

            for (const auto& candidate : qAsConst (candidates))
            {
                if (candidate != t && candidate->box ().intersects (box))
                {
                    auto p = candidate->plane ();

                    Segment segOfIntersection;

                    if (p.isValid ())
                    {
                        segOfIntersection = plane.intersection (p, 0.00000000000001);
                    }
                    bool intersect = false;

                    auto test = [] (const HalfEdge& he1, const HalfEdge& he2) -> bool {
                        auto res = intersectionOfLines3DMk2 (he1, he2);
                        return res.type == IntersectionOfLines3DMk2Result::Cross;
                    };

                    if (!segOfIntersection.isValid ())
                    {
                        // No intersection of planes - must be parallel or the same plane, or same plane inverted
                        if (plane.equal (p))
                        {
                            // Coplanar Ts
                            intersect = test (HalfEdge (candidate, 0), HalfEdge (t, 0)) || test (HalfEdge (candidate, 0), HalfEdge (t, 1)) || test (HalfEdge (candidate, 0), HalfEdge (t, 2)) || test (HalfEdge (candidate, 1), HalfEdge (t, 0)) ||
                                        test (HalfEdge (candidate, 1), HalfEdge (t, 1)) || test (HalfEdge (candidate, 1), HalfEdge (t, 2)) || test (HalfEdge (candidate, 2), HalfEdge (t, 0)) || test (HalfEdge (candidate, 2), HalfEdge (t, 1)) ||
                                        test (HalfEdge (candidate, 2), HalfEdge (t, 2));

                            if (!intersect)
                            {
                                auto res = t->containsWithDetails (candidate->centroid ());
                                intersect = res == Triangle::TriangleContainsResult::Contained;
                            }
                        }
                    }
                    else
                    {
                        tts.clear ();
                        cts.clear ();

                        bool resolved = false;
                        for (int e = 0; e < 3; e++)
                        {
                            auto tres = intersectionOfLines3D (segOfIntersection, HalfEdge (t, e));
                            //qDebug () << "target" << tres.toString ();
                            if (tres.intersects1() == IntersectionOfLinesResult3D::Colinear)
                            {
                                // Ts are not in the same plane, but share this one shares and edge with out plane intersect - cannot overlap
                                resolved = true;
                                break;
                            }
                            if (tres.vertexOfIntersection && 0.0 < tres.t2 && tres.t2 < 1.0)
                            {
                                auto res = t->containsWithDetails (tres.vertexOfIntersection);
                                switch (res)
                                {
                                case Triangle::TriangleContainsResult::External:
                                case Triangle::TriangleContainsResult::Edge0:
                                case Triangle::TriangleContainsResult::Edge1:
                                case Triangle::TriangleContainsResult::Edge2:
                                case Triangle::TriangleContainsResult::Vertex0:
                                case Triangle::TriangleContainsResult::Vertex1:
                                case Triangle::TriangleContainsResult::Vertex2:
                                    // Nothing
                                    break;
                                case Triangle::TriangleContainsResult::Contained:
                                    tts.push_back (tres.t1);
                                }
                            }
                            auto cres = intersectionOfLines3D (segOfIntersection, HalfEdge (candidate, e));
                            //qDebug () << "candidate" << cres.toString ();
                            if (cres.intersects1() == IntersectionOfLinesResult3D::Colinear)
                            {
                                // Ts are no in the same plane, but this one shares and edge with out plane intersect - cannot overlap
                                resolved = true;
                                break;
                            }
                            if (cres.vertexOfIntersection && 0.0 < cres.t2 && cres.t2 < 1.0)
                            {
                                if (candidate->contains(cres.vertexOfIntersection))
                                {
                                    cts.push_back (cres.t1);
                                }
                            }
                        }
                        if (!resolved && !tts.empty () && !cts.empty ())
                        {
                            std::sort (tts.begin (), tts.end ());
                            std::sort (cts.begin (), cts.end ());
                            constexpr double E = 0.00000000001;

                            // qDebug() << tts.constFirst() << tts.constLast() <<  tts.constFirst() - tts.constLast();
                            // qDebug() << cts.constFirst() << cts.constLast() <<  cts.constFirst() - cts.constLast();
                            intersect = !(cts.constLast () < tts.constFirst () + E || cts.constFirst () > tts.constLast () - E);

                            if (std::abs (cts.constFirst() - cts.constLast()) < E || std::abs (tts.constFirst() - tts.constLast()) < E)
                            {
                                // Must be tip touch - which is ok
                                intersect = false;
                            }
                        }
                    }

                    if (intersect)
                    {
                        if (t->id () < candidate->id ())
                        {
                            overlaps.push_back ({t, candidate});
                        }
                        else
                        {
                            overlaps.push_back ({candidate, t});
                        }
                    }
                }
                // TODO: what if there is no common intersection of planes (empty T or co-planar Ts);
            }
        }
    }
    if (!overlaps.isEmpty ())
    {
        std::sort (overlaps.begin (), overlaps.end (), [] (const auto& a, const auto& b) -> bool {
            if (a.first->id () != b.first->id ())
            {
                return a.first->id () < b.first->id ();
            }
            return a.second->id () < b.second->id ();
        });
        auto it = std::unique (overlaps.begin (), overlaps.end (), [] (const auto& a, const auto& b) -> bool {
            return a.first->id () == b.first->id () && a.second->id () == b.second->id ();
        });
        overlaps.erase (it, overlaps.end ());

        for (auto& overlap : overlaps)
        {
            PolygonCorefiner pcr;
            pcr.corefine (overlap.first->toPolygon (), overlap.second->toPolygon ());

            auto res = pcr.boolIntersection ();
            if (!res.isEmpty ())
            {
                overlap.area = res.constFirst ().area ();
            }
        }

        std::sort (overlaps.begin (), overlaps.end (), [] (const auto& a, const auto& b) -> bool {
            return ! a.area < b.area;
        });

        for (const auto& overlap : overlaps)
        {
            PolygonCorefiner pcr;
            pcr.corefine (overlap.first->toPolygon (), overlap.second->toPolygon ());

            ret.push_back (QStringLiteral ("    Overlap: %1 and %2 (%3sq)\n").arg (fmtName (overlap.first), fmtName (overlap.second)).arg (overlap.area));
            if (m_callback)
            {
                m_callback (CheckTriangleOverlap, m_mesh, overlap.first, overlap.second);
            }
#if 0
            {
                auto mesh = Mesh::create ();
                mesh->add(overlap.first);
                mesh->add (overlap.second);
                static int cnt;
                Document::write(mesh, QStringLiteral ("/tmp/3d/overlap%1.nethers").arg (cnt++, 3, 10, QChar('0')));
            }
#endif
        }

        ret.push_front (QStringLiteral ("  Found %1 triangle overlaps\n").arg (overlaps.count ()));
    }
    else
    {
        ret += QStringLiteral ("  No triangle overlap\n");
    }
    return {CheckTriangleOverlap, overlaps.isEmpty (), ret, (int)overlaps.count ()};
}

CheckResult MeshChecker::checkUnviableTriangles ()
{
    QString ret;
    int badCount = 0;
    for (const auto& t : *m_mesh)
    {
        if (!t->isViable ())
        {
            ret += QStringLiteral ("    T: %1\n").arg (fmtName (t));
            badCount++;
        }
    }
    if (!badCount)
    {
        return {CheckUnviableTriangles, true, QStringLiteral ("  No unviable triangles\n"), badCount};
    }
    ret.push_front (QStringLiteral ("  %1 unviable (small or flat) triangles\n").arg (badCount));
    return {CheckUnviableTriangles, true, ret, badCount};
}

CheckResult MeshChecker::checkFlatTriangles ()
{
    QString ret;
    int badCount = 0;
    for (const auto& t : *m_mesh)
    {
        auto tnorm = t->unitNormal ();
        if (!tnorm)
        {
            badCount++;
            ret += QStringLiteral ("    T: %1\n").arg (fmtName (t));
        }
    }
    if (badCount)
    {
        ret.push_front (QStringLiteral ("  %1 flat triangles\n").arg (badCount));
        return {CheckFlatTriangles, false, ret, badCount};
    }
    return {CheckFlatTriangles, true, QStringLiteral ("  No flat triangles\n"), badCount};
}

CheckResult MeshChecker::checkTCount ()
{
    return {CheckTCount, true, {}, m_mesh->tcount ()};
}


CheckResult MeshChecker::checkComponents ()
{
    auto no = (int)m_mesh->splitComponents().count();
    return {CheckComponents, true, {}, no};
}

CheckResult MeshChecker::checkAnnotations ()
{
    QString ret;
    int badCount = 0;
    QHash<QString, TrianglePtr> hash;
    for (const auto& t : *m_mesh)
    {
        if (!t->annotation ().isEmpty ())
        {
            if (hash.contains (t->annotation ()))
            {
                auto tt = hash.value (t->annotation ());
                badCount++;
                ret += QStringLiteral ("    T: %1 & %2  (\"%3\")\n").arg (fmtName(t), fmtName (tt), t->annotation ());
            }
            else
            {
                hash.insert (t->annotation (), t);
            }
        }
    }

    if (badCount)
    {
        ret.push_front (QStringLiteral ("  %1 duplicate triangle annotations\n").arg (badCount));
        return {CheckDuplicateAnnotations, false, ret, badCount};
    }
    return {CheckDuplicateAnnotations, true, QStringLiteral ("  No duplicate triangle annotations\n"), badCount};
}

QString MeshChecker::checkName (Checks check)
{
    switch (check)
    {
    case CheckNothing:
        return QStringLiteral ("Nothing");
    case CheckHoles:
        return QStringLiteral ("Holes");
    case CheckDuplicateTriangles:
        return QStringLiteral ("DuplicateTriangles");
    case CheckShortEdges:
        return QStringLiteral ("ShortEdges");
    case CheckInfo:
        return QStringLiteral ("Info");
    case CheckReversedTriangles:
        return QStringLiteral ("ReversedTriangles");
    case CheckDuplicateVertices:
        return QStringLiteral ("DuplicateVertices");
    case CheckOpenEdges:
        return QStringLiteral ("OpenEdges");
    case CheckTriangleOverlap:
        return QStringLiteral ("OverlapTriangles");
    case CheckUnviableTriangles:
        return QStringLiteral ("UnviableTriangles");
    case CheckOverusedHalfEdges:
        return QStringLiteral ("OverusedHalfEdges");
    case CheckFlatTriangles:
        return QStringLiteral ("FlatTriangles");
    case CheckDeleted:
        return QStringLiteral ("Deleted");
    case CheckVertexLowRefs:
        return QStringLiteral ("VertexLowRefs");
    case CheckTCount:
        return QStringLiteral ("TCount");
    case CheckDuplicateAnnotations:
        return QStringLiteral ("DuplicateAnnotations");
    case CheckComponents:
        return QStringLiteral("Components");
    default:
        return QStringLiteral ("??");
    }
}

QString MeshChecker::optionName (Checks check)
{
    return QStringLiteral ("Check") + checkName (check);
}

QString MeshChecker::description (Checks check)
{
    switch (check)
    {
    case CheckNothing:
        return QStringLiteral ("Nothing");
    case CheckHoles:
        return QStringLiteral ("Check for holes");
    case CheckDuplicateTriangles:
        return QStringLiteral ("Check for duplicate triangles");
    case CheckShortEdges:
        return QStringLiteral ("Check for short edges");
    case CheckInfo:
        return QStringLiteral ("Show basic stats");
    case CheckReversedTriangles:
        return QStringLiteral ("Check for triangles who's edges are in the same direction as its neighbours - i.e. the triangle is reversed");
    case CheckDuplicateVertices:
        return QStringLiteral ("Check for duplicate vertices");
    case CheckOpenEdges:
        return QStringLiteral ("Check for open edges");
    case CheckTriangleOverlap:
        return QStringLiteral ("Check for overlapping triangles");
    case CheckUnviableTriangles:
        return QStringLiteral ("Check for triangles for expressively small heights and edges");
    case CheckOverusedHalfEdges:
        return QStringLiteral ("Check for overused half edges");
    case CheckFlatTriangles:
        return QStringLiteral ("Check for flat triangles (too flat to reliably calculate a normal)");
    case CheckDeleted:
        return QStringLiteral ("Check for deleted triangles (nethers format only)");
    case CheckVertexLowRefs:
        return QStringLiteral ("Check for vertices with too few references");
    case CheckTCount:
        return QStringLiteral ("Check - report number of triangles");
    case CheckDuplicateAnnotations:
        return QStringLiteral ("Check for duplicate triangle annotations (nethers format only)");
    case CheckComponents:
        return QStringLiteral("Component count");
    default:
        return QStringLiteral ("??");
    }
}

bool MeshChecker::failable (Checks check)
{
    switch (check)
    {
    case CheckNothing:
    case CheckShortEdges:
    case CheckInfo:
    case CheckUnviableTriangles:
    case CheckFlatTriangles:
    case CheckTCount:
    case CheckComponents:
        return false;

    case CheckHoles:
    case CheckDuplicateTriangles:
    case CheckReversedTriangles:
    case CheckDuplicateVertices:
    case CheckOpenEdges:
    case CheckTriangleOverlap:
    case CheckOverusedHalfEdges:
    case CheckDeleted:
    case CheckVertexLowRefs:
    case CheckDuplicateAnnotations:
        return true;

    default:
        Q_ASSERT (false);
        return true;
    }
}

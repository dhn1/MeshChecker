#include "MeshChecker.h"

#include <ColourFactory.h>
#include <Constants.h>
#include <DocumentNethers.h>
#include <HalfEdges/HalfEdges.h>
#include <Mesh.h>
#include <Plane.h>
#include <Polygon2DList.h>
#include <SegmentList.h>
#include <TriangleOctTree/TriangleOctTree.h>

#include <QMutex>
#include <QMutexLocker>
#include <QtConcurrent>

#include "globals.h"

static QMutex flagMutex;  // Used to control access to triangle flags
MeshChecker::CheckList MeshChecker::m_checkList;

MeshChecker::MeshChecker (const MeshPtr& mesh, const QString& path) : m_mesh (mesh)
{
    m_path = path;
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

const MeshChecker::CheckList& MeshChecker::checkList ()
{
    if (m_checkList.isEmpty ())
    {
        m_checkList.push_back ({CheckInfo, &MeshChecker::checkInfo});
        m_checkList.push_back ({CheckHoles, &MeshChecker::checkHoles});
        m_checkList.push_back ({CheckDuplicateTriangles, &MeshChecker::checkDuplicateTriangles});
        m_checkList.push_back ({CheckShortEdges, &MeshChecker::checkShortEdges});
        m_checkList.push_back ({CheckReversedTriangles, &MeshChecker::checkReversedTriangles});
        m_checkList.push_back ({CheckDuplicateVertices, &MeshChecker::checkDuplicateVertices});
        m_checkList.push_back ({CheckOpenEdges, &MeshChecker::checkOpenEdges});
        m_checkList.push_back ({CheckHalfEdgeOverlap, &MeshChecker::checkHalfEdgeOverlap});
        m_checkList.push_back ({CheckTriangleOverlap, &MeshChecker::checkTriangleOverlap});
        m_checkList.push_back ({CheckUnviableTriangles, &MeshChecker::checkUnviableTriangles});
        m_checkList.push_back ({CheckOverusedHalfEdges, &MeshChecker::checkOverusedEdges});
        m_checkList.push_back ({CheckFlatTriangles, &MeshChecker::checkFlatTriangles});
        m_checkList.push_back ({CheckDeleted, &MeshChecker::checkDeleted});
        m_checkList.push_back ({CheckVertexLowRefs, &MeshChecker::checkVertexRefs});
    }
    return m_checkList;
}

MeshChecker::~MeshChecker ()
{
    m_octtreeFuture.waitForFinished ();
    m_edgesFuture.waitForFinished ();
    delete m_octtreeFuture.result ();
}

bool MeshChecker::checkMultiThreaded ()
{
    QList<QFuture<CheckRet>> futures;

    if (m_checks & CheckInfo)
    {
        auto res = QtConcurrent::run ([this] {
            return checkInfo ();
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

    if (m_checks & CheckUnviableTriangles)
    {
        auto res = QtConcurrent::run ([this] {
            return checkUnviableTriangles ();
        });
        futures.push_back (res);
    }

    if (m_checks & CheckOverusedHalfEdges)
    {
        auto res = QtConcurrent::run ([this] {
            return checkOverusedEdges ();
        });
        futures.push_back (res);
    }

    if (m_checks & CheckFlatTriangles)
    {
        auto res = QtConcurrent::run ([this] {
            return checkFlatTriangles ();
        });
        futures.push_back (res);
    }

    bool ret = true;
    for (const auto& f : futures)
    {
        auto res = f.result ();
        report (res);
        ret &= f.result ().first;
    }

    m_octtreeFuture.waitForFinished ();
    m_edgesFuture.waitForFinished ();

    QThread::usleep (20);  // TODO: remove?
    return ret;
}

bool MeshChecker::check ()
{
    QLocale locale;
    constexpr int padding = 20;
    bool ret = true;
    m_summary.clear ();

    for (const auto& c : m_checkList)
    {
        if (m_checks & c.first)
        {
            auto res = (this->*c.second) ();
            ret &= res.first;
            report (res);
            if (res.m_badCount >= 0)
            {
                const auto& name = checkName (c.first);
                m_summary += "    " + name + QString (padding - name.length (), QChar ('.')) + ": " + locale.toString (res.m_badCount) + "\n";
            }
        }
    }

    return ret;
}

void MeshChecker::report (const CheckRet& res)
{
    if (verbosity & Details)
    {
        out << res.second;
    }
    else if (verbosity & Summary)
    {
        out << res.second.split ('\n').constFirst () << '\n';
    }
    out.flush ();
}

HalfEdgesPtr MeshChecker::getEdges ()
{
    if (!m_edgesPtr)
    {
        m_edgesPtr = m_edgesFuture.result ();
    }
    return m_edgesPtr;
}

MeshChecker::CheckRet MeshChecker::checkVertexRefs ()
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

        return {false, str, badCount};
    }
    str.push_front (QStringLiteral ("  No low ref vertices found\n"));
    return {true, str, badCount};
}

MeshChecker::CheckRet MeshChecker::checkDeleted ()
{
    QString str;
    int badCount = 0;
    for (const auto& t : *m_mesh)
    {
        if (t->testFlag (Triangle::Delete))
        {
            badCount++;
            str += QStringLiteral ("    %1\n").arg (t->name ());
        }
    }
    if (badCount)
    {
        str.push_front (QStringLiteral ("  %1 deleted triangles found\n").arg (badCount));

        return {false, str, badCount};
    }
    str.push_front (QStringLiteral ("  No deleted triangles found\n"));
    return {true, str, badCount};
}

QString MeshChecker::summary () const
{
    return m_summary;
}

MeshChecker::CheckRet MeshChecker::checkOpenEdges ()
{
    QString str;

    auto edges = getEdges ();
    int badCount = 0;

    for (const auto& e : edges->openEdges ())
    {
        if (!e->testFlag (HalfEdge::Delete))
        {
            str += QStringLiteral ("    T: %1, Edge: %2 -> %3\n").arg (e->triangle ()->name ()).arg (e->v1 ()->name ()).arg (e->v2 ()->name ());
            badCount++;
        }
    }
    if (badCount == 0)
    {
        str.push_front (QStringLiteral ("  No open edges found\n"));
        return {true, str, badCount};
    }
    str.push_front (QStringLiteral ("  %1 Open edges found\n").arg (badCount));
    return {badCount == 0, str, badCount};
}

MeshChecker::CheckRet MeshChecker::checkHoles ()
{
    QMutexLocker locker (&flagMutex);

    auto holes = getEdges ()->holes ();
    if (holes.isEmpty ())
    {
        return {true, QStringLiteral ("  No holes\n"), 0};
    }
    QString r;

    r = QStringLiteral ("  %1 Open holes found\n").arg (holes.count ());

    //int idx = -1;
    for (const auto& hole : qAsConst (holes))
    {
        //idx++;
        auto area = hole.area ();
        r += QStringLiteral ("    Hole: (") + QString::number (area) + "sq)\n";
    }
    return {holes.isEmpty (), r, (int)holes.count ()};
}

MeshChecker::CheckRet MeshChecker::checkDuplicateTriangles ()
{
    auto count = m_mesh->count ();
    const auto& ttree = m_octtreeFuture.result ();
    QString r;
    int badCount = 0;

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
                r += QStringLiteral ("    T: %1 and T: %2\n").arg (t->name ()).arg (tt->name ());
                badCount++;
            }
        }
    }
    if (!badCount)
    {
        r += QStringLiteral ("  No duplicate triangles\n");
        return {true, r, badCount};
    }
    r.push_front (QStringLiteral ("  %1 duplicate triangles\n").arg (badCount / 2));
    return {false, r, badCount};
}

MeshChecker::CheckRet MeshChecker::checkShortEdges ()
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
                ret += "    Null edge (repeating vertex): " + edge->v1 ()->toString () + "\n";
            }
            auto mag2 = edge->magnitude2 ();
            if (mag2 <= Constants::minEdge2)
            {
                foundShortEdge++;
                if (edge->pair ())
                {
                    ret +=
                        "    Short edge " + QString::number (sqrt (mag2)) + "mm. Between vertex: " + edge->v1 ()->name () + " and " + edge->v2 ()->name () + ". Ts " + edge->triangle ()->name () + " and " + edge->pair ()->triangle ()->name () + " \n";
                }
                else
                {
                    ret += "    Short edge " + QString::number (sqrt (mag2)) + "mm. Between vertex: " + edge->v1 ()->name () + " and " + edge->v2 ()->name () + ". Ts " + edge->triangle ()->name () + " and None\n";
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
    return {ok, ret, -1};
}

MeshChecker::CheckRet MeshChecker::checkReversedTriangles ()
{
    QString ret;

    QMutexLocker locker (&flagMutex);

    QList<TrianglePtr> reversedTriangles;
    const auto hash = getEdges ()->edgeByEdge ();

    for (const auto& t : *m_mesh)
    {
        for (int ee = 0; ee < 3; ee++)
        {
            auto const & e = t->halfEdge (ee);

            if (e->testFlag (HalfEdge::Delete))
            {
                continue;
            }

            auto values = hash.values ({e});
            //Q_ASSERT (values.count () == 1 || values.count() == 2);

            if (values.count () == 2)
            {
                if (values.at (0)->v1 () == values.at (1)->v1 ())
                {
                    reversedTriangles << e->triangle ();
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
            ret += QStringLiteral ("    triangle: %1\n").arg (t->name ());
            badCount++;
        }
        i += count;
    }
    if (badCount)
    {
        ret.push_front (QStringLiteral ("  %1 reversed triangles\n").arg (badCount));
        return {false, ret, badCount};
    }
    ret += QStringLiteral ("  No reversed triangles\n");
    return {true, ret, badCount};
}

MeshChecker::CheckRet MeshChecker::checkOverusedEdges ()
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
            auto e = values.constFirst ();
            ret.push_back (QStringLiteral ("    edge: %1 -> %2\n").arg (e->v1 ()->name ()).arg (e->v2 ()->name ()));
            for (const auto& match : values)
            {
                ret.push_back (QStringLiteral ("      Used by T: %1\n").arg (match->triangle ()->name ()));
            }
        }
    }

    if (badCount)
    {
        ret.push_front (QStringLiteral ("  %1 overused half edges\n").arg (badCount));
        return {false, ret, badCount};
    }
    ret += QStringLiteral ("  No overused half edges\n");
    return {true, ret, badCount};
}

MeshChecker::CheckRet MeshChecker::checkInfo ()
{
    const QLocale locale;
    QString ret;
    auto vcount = m_mesh->vertexList ().count ();
    auto edges = getEdges ();
    auto ecount = m_mesh->halfEdgeList ().count ();

    QMutexLocker locker (&flagMutex);

    QFileInfo f (m_path);
    ret += QStringLiteral ("  Info\n    Modified: %1\n").arg (f.lastModified ().toString ());

    const auto comps = m_mesh->splitComponents (edges);
    if (comps.count () < 2)
    {
        ret += QStringLiteral ("    %1 triangles, %2 vertices, %3 half edges\n")
                  .arg (locale.toString (m_mesh->tcount ()))
                  .arg (locale.toString (vcount))
                  .arg (locale.toString (ecount));
    }
    else
    {
        ret += QStringLiteral ("    %4 components, %1 triangles, %2 vertices, %3 half edges\n")
                  .arg (locale.toString (m_mesh->tcount ()))
                  .arg (locale.toString (vcount))
                  .arg (locale.toString (ecount))
                  .arg(comps.count())
                  ;

        int idx = 0;
        for (const auto& comp : comps)
        {
            auto vcount = comp->vertexList ().count ();
            auto edges = HalfEdges::create (comp);
            auto ecount = comp->halfEdgeList().count();

            ret += QStringLiteral ("      comp %4: %1 triangles, %2 vertices, %3 half edges\n")
                       .arg (locale.toString (comp->tcount ()))
                       .arg (locale.toString (vcount))
                       .arg (locale.toString (ecount))
                       .arg (idx++);

            if (comp->count () < 10)
            {
                for (const auto& t : *comp)
                {
                    ret += QStringLiteral ("        T: %1\n").arg (t->name ());
                }
            }
        }
    }
    return {true, ret, -1};
}

MeshChecker::CheckRet MeshChecker::checkDuplicateVertices ()
{
    QMutexLocker locker (&flagMutex);
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
            ret += QStringLiteral ("    %1 and %2\n").arg (v->name ()).arg (res->name ());
        }
    }
    if (badCount)
    {
        ret.prepend (QStringLiteral ("  %1 duplicate vertices\n").arg (badCount));
        return {false, ret, badCount};
    }
    return {true, "  No duplicate vertices\n", badCount};
}

MeshChecker::CheckRet MeshChecker::checkHalfEdgeOverlap ()
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
    const auto& ttree = *m_octtreeFuture.result ();
    //QSet <QPair<SegmentPtr, SegmentPtr>> reported;
    for (const auto& t : qAsConst (*m_mesh))
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
                    case IntersectionOfLines3DMk2Result::InLine:  // Unreliable
                    case IntersectionOfLines3DMk2Result::TJunct:  // Unreliable
                        // badCount++;
                        // ret += QStringLiteral ("    T junction edges: %1 -> %2 and %3 -> %4\n").arg (vname (segs[e]->start ())).arg (vname (segs[e]->end ())).arg (vname (seg->start ())).arg (vname (seg->end ()));
                        break;
                    case IntersectionOfLines3DMk2Result::Cross:
                        // if (reported.contains({seg, segs[e]}))
                        // {
                        //     continue;
                        // }
                        badCount++;
                        ret.push_back (QStringLiteral ("    Intersecting edges: %1 -> %2 and %3 -> %4\n").arg (segs[ee]->start ()->name ()).arg (segs[ee]->end ()->name ()).arg (seg->start ()->name ()).arg (seg->end ()->name ()));
                        //reported.insert ({seg, segs[e]});
                        break;
                    }
                }
            }
        }
    }
#endif
    ret.push_front (QStringLiteral ("  Found %1 overlapping halfEdges (currently each reported twice).\n").arg (badCount / 2));
    return {badCount == 0, ret, badCount / 2};
}

MeshChecker::CheckRet MeshChecker::checkTriangleOverlap ()
{
    QString ret;
    QList<double> tts;
    QList<double> cts;
    const auto& ttree = *m_octtreeFuture.result ();
    QMutexLocker locker (&flagMutex);
    QList<QPair<TrianglePtr, TrianglePtr>> overlaps;

    for (const auto& t : qAsConst (*m_mesh))
    {
        auto box = t->box ();
        auto plane = t->plane ();
        auto candidates = ttree.find (box);
        m_mesh->unsetFlag (Triangle::Tagged);

        for (const auto& c : qAsConst (candidates))
        {
            if (c != t && c->box ().intersects (box))
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
                        intersect = test (HalfEdge (c, 0), HalfEdge (t, 0)) || test (HalfEdge (c, 0), HalfEdge (t, 1)) || test (HalfEdge (c, 0), HalfEdge (t, 2)) || test (HalfEdge (c, 1), HalfEdge (t, 0)) || test (HalfEdge (c, 1), HalfEdge (t, 1)) ||
                                    test (HalfEdge (c, 1), HalfEdge (t, 2)) || test (HalfEdge (c, 2), HalfEdge (t, 0)) || test (HalfEdge (c, 2), HalfEdge (t, 1)) || test (HalfEdge (c, 2), HalfEdge (t, 2));

                        if (!intersect)
                        {
                            auto res = t->containsWithDetails (c->centroid ());
                            intersect = res == Triangle::TriangleContainsResult::Contained;
                        }
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
                    if (t->id () < c->id ())
                    {
                        overlaps.push_back ({t, c});
                    }
                    else
                    {
                        overlaps.push_back ({c, t});
                    }
                }
            }
        }
    }

    if (!overlaps.isEmpty ())
    {
        std::sort (overlaps.begin (), overlaps.end ());
        auto it = std::unique (overlaps.begin (), overlaps.end ());
        overlaps.erase (it, overlaps.end ());

        for (const auto& overlap : overlaps)
        {
            ret.push_back (QStringLiteral ("    Overlap: %1 and %2\n").arg (overlap.first->name ()).arg (overlap.second->name ()));
        }

        ret.push_front (QStringLiteral ("  Found %1 triangle overlaps\n").arg (overlaps.count ()));
    }
    else
    {
        ret += QStringLiteral ("  No triangle overlap\n");
    }
    return {overlaps.isEmpty (), ret, (int)overlaps.count ()};
}

MeshChecker::CheckRet MeshChecker::checkUnviableTriangles ()
{
    QString ret;
    int badCount = 0;
    for (const auto& t : *m_mesh)
    {
        if (!t->isViable ())
        {
            ret += QStringLiteral ("    T: %1\n").arg (t->name ());
            badCount++;
        }
    }
    if (!badCount)
    {
        return {true, "  No unviable triangles\n", badCount};
    }
    ret.push_front (QStringLiteral ("  %1 unviable (small or flat) triangles\n").arg (badCount));
    return {true, ret, badCount};
}

MeshChecker::CheckRet MeshChecker::checkFlatTriangles ()
{
    QString ret;
    int badCount = 0;
    for (const auto& t : *m_mesh)
    {
        auto tnorm = t->unitNormal ();
        if (!tnorm)
        {
            badCount++;
            ret += QStringLiteral ("    T: %1\n").arg (t->name ());
        }
    }
    if (badCount)
    {
        ret.push_front (QStringLiteral ("  %1 flat triangles\n").arg (badCount));
        return {false, ret, badCount};
    }
    return {true, "  No flat triangles\n", badCount};
}

QString MeshChecker::checkName (MeshChecker::Checks check)
{
    switch (check)
    {
    case MeshChecker::CheckNothing:
        return "Nothing";
    case MeshChecker::CheckHoles:
        return "Holes";
    case MeshChecker::CheckDuplicateTriangles:
        return "DuplicateTriangles";
    case MeshChecker::CheckShortEdges:
        return "ShortEdges";
    case MeshChecker::CheckInfo:
        return "Info";
    case MeshChecker::CheckReversedTriangles:
        return "ReversedTriangles";
    case MeshChecker::CheckDuplicateVertices:
        return "DuplicateVertices";
    case MeshChecker::CheckOpenEdges:
        return "OpenEdges";
    case MeshChecker::CheckHalfEdgeOverlap:
        return "HalfEdgeOverlap";
    case MeshChecker::CheckTriangleOverlap:
        return "TriangleOverlap";
    case MeshChecker::CheckUnviableTriangles:
        return "UnviableTriangles";
    case MeshChecker::CheckOverusedHalfEdges:
        return "OverusedHalfEdges";
    case MeshChecker::CheckFlatTriangles:
        return "FlatTriangles";
    case MeshChecker::CheckDeleted:
        return "Deleted";
    case MeshChecker::CheckVertexLowRefs:
        return "VertexLowRefs";
    default:
        return "??";
    }
}

QString MeshChecker::optionName (MeshChecker::Checks check)
{
    return QStringLiteral ("Check") + checkName (check);
}

QString MeshChecker::description (MeshChecker::Checks check)
{
    switch (check)
    {
    case MeshChecker::CheckNothing:
        return "Nothing";
    case MeshChecker::CheckHoles:
        return "check for holes";
    case MeshChecker::CheckDuplicateTriangles:
        return "check for duplicate triangles";
    case MeshChecker::CheckShortEdges:
        return "check for short edges";
    case MeshChecker::CheckInfo:
        return "show basic stats";
    case MeshChecker::CheckReversedTriangles:
        return "check for reversed triangles";
    case MeshChecker::CheckDuplicateVertices:
        return "check for duplicate vertices";
    case MeshChecker::CheckOpenEdges:
        return "check for open edges";
    case MeshChecker::CheckHalfEdgeOverlap:
        return "check for overlapping half edges";
    case MeshChecker::CheckTriangleOverlap:
        return "check for overlapping triangles";
    case MeshChecker::CheckUnviableTriangles:
        return "check for unviable triangles";
    case MeshChecker::CheckOverusedHalfEdges:
        return "check for overused half edges";
    case MeshChecker::CheckFlatTriangles:
        return "check for flat triangles";
    case MeshChecker::CheckDeleted:
        return "Check for deleted triangles (nethers only)";
    case MeshChecker::CheckVertexLowRefs:
        return "check for vertices with too few references";
    default:
        return "??";
    }
}

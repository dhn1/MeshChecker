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

static QMutex flagMutex;  // Used to controll acces to triangle flags

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
    delete m_octtreeFuture.result ();
}

bool MeshChecker::checkMultiThreaded ()
{
    QList<QFuture<QPair<bool, QString>>> futures;

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

    if (m_checks & CheckOverusedEdges)
    {
        auto res = QtConcurrent::run ([this] {
            return checkOverusedEdges ();
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
    bool ret = true;

    if (m_checks & CheckInfo)
    {
        auto res = checkInfo ();
        ret &= res.first;
        report (res);
    }
    if (m_checks & CheckHoles)
    {
        auto res = checkHoles ();
        ret &= res.first;
        report (res);
    }
    if (m_checks & CheckDuplicateTriangles)
    {
        auto res = checkDuplicateTriangles ();
        ret &= res.first;
        report (res);
    }
    if (m_checks & CheckShortEdges)
    {
        auto res = checkShortEdges ();
        ret &= res.first;
        report (res);
    }

    if (m_checks & CheckReversedTriangles)
    {
        auto res = checkReversedTriangles ();
        ret &= res.first;
        report (res);
    }

    if (m_checks & CheckDuplicateVertices)
    {
        auto res = checkDuplicateVertices ();
        ret &= res.first;
        report (res);
    }

    if (m_checks & CheckOpenEdges)
    {
        auto res = checkOpenEdges ();
        ret &= res.first;
        report (res);
    }

    if (m_checks & CheckHalfEdgeOverlap)
    {
        auto res = checkHalfEdgeOverlap ();
        ret &= res.first;
        report (res);
    }

    if (m_checks & CheckTriangleOverlap)
    {
        auto res = checkTriangleOverlap ();
        ret &= res.first;
        report (res);
    }

    if (m_checks & CheckUnviableTriangles)
    {
        auto res = checkUnviableTriangles ();
        ret &= res.first;
        report (res);
    }

    if (m_checks & CheckOverusedEdges)
    {
        auto res = checkOverusedEdges ();
        ret &= res.first;
        report (res);
    }
    return ret;
}

void MeshChecker::report (QPair<bool, QString>& res)
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

EdgesPtr MeshChecker::getEdges ()
{
    if (!m_edgesPtr)
    {
        m_edgesPtr = m_edgesFuture.result ();
    }
    return m_edgesPtr;
}

QPair<bool, QString> MeshChecker::checkOpenEdges ()
{
    QString str;

    auto edges = getEdges ();
    int badCount = 0;

    for (const auto& e : m_mesh->openEdges ())
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
        return {true, str};
    }
    str.push_front (QStringLiteral ("  %1 Open edges found\n").arg (badCount));
    return {badCount == 0, str};
}

QPair<bool, QString> MeshChecker::checkHoles ()
{
    QMutexLocker locker (&flagMutex);

    auto holes = getEdges ()->holes ();
    if (holes.isEmpty ())
    {
        return {true, QStringLiteral ("  No holes\n")};
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
    return {holes.isEmpty (), r};
}

QPair<bool, QString> MeshChecker::checkDuplicateTriangles ()
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
        return {true, r};
    }
    r.push_front (QStringLiteral ("  %1 duplicate triangles\n").arg (badCount / 2));
    return {false, r};
}

QPair<bool, QString> MeshChecker::checkShortEdges ()
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
    return {ok, ret};
}

QPair<bool, QString> MeshChecker::checkReversedTriangles ()
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
        return {false, ret};
    }
    ret += QStringLiteral ("  No reversed triangles\n");
    return {true, ret};
}

QPair<bool, QString> MeshChecker::checkOverusedEdges ()
{
    QString ret;
    int badCount = 0;
    const auto& edgeByEdge = getEdges ()->edgeByEdge ();

    auto keys = edgeByEdge.keys ();
    std::sort (keys.begin (), keys.end ());
    auto it = std::unique (keys.begin (), keys.end ());
    keys.erase (it, keys.end ());

    for (const auto& key : keys)
    {
        auto values = edgeByEdge.values (key);
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
        return {false, ret};
    }
    ret += QStringLiteral ("  No overused half edges\n");
    return {true, ret};
}

QPair<bool, QString> MeshChecker::checkInfo ()
{
    const QLocale locale;
    QString ret;
    auto vcount = m_mesh->vertexList ().count ();
    auto edges = getEdges ();
    auto ecount = m_mesh->halfEdgeList ().count ();

    QMutexLocker locker (&flagMutex);

    const auto comps = m_mesh->splitComponents (edges);
    if (comps.count () < 2)
    {
        ret = QStringLiteral ("  %1 triangles, %2 vertices, %3 half edges (%4 edges)\n").arg (locale.toString (m_mesh->tcount ())).arg (locale.toString (vcount)).arg (locale.toString (ecount)).arg (locale.toString (ecount / 2));
    }
    else
    {
        ret = QStringLiteral ("  %5 components, %1 triangles, %2 vertices, %3 half edges (%4 edges)\n")
                  .arg (locale.toString (m_mesh->tcount ()))
                  .arg (locale.toString (vcount))
                  .arg (locale.toString (ecount))
                  .arg (locale.toString (ecount / 2))
                  .arg (locale.toString (comps.count ()));
        int idx = 0;
        for (const auto& comp : comps)
        {
            auto vcount = comp->vertexList ().count ();
            auto edges = HalfEdges::create (comp);
            auto ecount = m_mesh->halfEdgeList ().count ();

            ret += QStringLiteral ("    comp %5: %1 triangles, %2 vertices, %3 half edges (%4 edges)\n")
                       .arg (locale.toString (comp->tcount ()))
                       .arg (locale.toString (vcount))
                       .arg (locale.toString (ecount))
                       .arg (locale.toString (ecount / 2))
                       .arg (idx++);
        }
    }
    return {true, ret};
}

QPair<bool, QString> MeshChecker::checkDuplicateVertices ()
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
        return {false, ret};
    }
    return {true, "  No duplicate vertices\n"};
}

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
    return {badCount == 0, ret};
}

QPair<bool, QString> MeshChecker::checkTriangleOverlap ()
{
    QString ret;
    int badCount = 0;
    QList<double> tts;
    QList<double> cts;
    const auto& ttree = *m_octtreeFuture.result ();
    QMutexLocker locker (&flagMutex);

    for (const auto& t : qAsConst (*m_mesh))
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
                        intersect = test (HalfEdge (c, 0), HalfEdge (t, 0)) || test (HalfEdge (c, 0), HalfEdge (t, 1)) || test (HalfEdge (c, 0), HalfEdge (t, 2)) || test (HalfEdge (c, 1), HalfEdge (t, 0)) || test (HalfEdge (c, 1), HalfEdge (t, 1)) ||
                                    test (HalfEdge (c, 1), HalfEdge (t, 2)) || test (HalfEdge (c, 2), HalfEdge (t, 0)) || test (HalfEdge (c, 2), HalfEdge (t, 1)) || test (HalfEdge (c, 2), HalfEdge (t, 2));
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
                    ret.push_back (QStringLiteral ("    Intersecting Ts: %1 and %2\n").arg (t->name ()).arg (c->name ()));
                    badCount++;
                }
                t->setFlag (Triangle::Tagged);
            }
        }
    }
    if (badCount)
    {
        ret.push_front (QStringLiteral ("  Found %1 intersecting triangles, out of %2.\n").arg (badCount).arg (m_mesh->count ()));
    }
    else
    {
        ret += QStringLiteral ("  No intersecting triangles\n");
    }
    m_mesh->unsetFlag (Triangle::Tagged);

    return {badCount == 0, ret};
}

QPair<bool, QString> MeshChecker::checkUnviableTriangles ()
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
        return {true, "  No unviable triangles\n"};
    }
    ret.push_front (QStringLiteral ("  %1 unviable (small or flat) triangles\n").arg (badCount));
    return {true, ret};
}

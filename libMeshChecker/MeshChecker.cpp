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

// Statics
MeshChecker::CheckList MeshChecker::m_checkList;
bool MeshChecker::m_viewerHyperlinks;
bool MeshChecker::m_allowOverusedEdges;

int MeshChecker::m_maxMessages = 20;

MeshChecker::MeshChecker ()
{
}

MeshChecker::MeshChecker (const MeshPtr& mesh, const QString& path) : m_mesh (mesh), m_path (path)
{
    genMetaData ();
}

void MeshChecker::setData (const MeshPtr& mesh, const QString& path)
{
    m_mesh = mesh;
    m_path = path;
    genMetaData ();
}

void MeshChecker::genMetaData ()
{
    auto future = QtConcurrent::run ([this] {
        auto ret = HalfEdges::create (m_mesh);
        return ret;
    });

    m_meshBox = m_mesh->box ();
    if (m_mesh->isEmpty ())
    {
        m_octtree = new TriangleOctTree (Box (10, 10, 10, 10, 10, 10));
    }
    else
    {
        m_octtree = new TriangleOctTree (m_meshBox);
        m_octtree->add (m_mesh);
    }
    m_edges = future.result ();
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

QString MeshChecker::fmtName (const HolePtr& hole)
{
    if (m_viewerHyperlinks)
    {
        return QStringLiteral ("![%1](/H/%2)").arg (hole->id ()).arg (hole->hash ());
    }
    return QStringLiteral ("%1").arg (hole->id ());
}

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
        m_checkList.push_back ({CheckOverlappingTriangles, &MeshChecker::checkOverlappingTriangles});
        m_checkList.push_back ({CheckDeleted, &MeshChecker::checkDeleted});
        m_checkList.push_back ({CheckVertexLowRefs, &MeshChecker::checkVertexRefs});
        m_checkList.push_back ({CheckUnviableTriangles, &MeshChecker::checkUnviableTriangles});
        m_checkList.push_back ({CheckFlatTriangles, &MeshChecker::checkFlatTriangles});
        m_checkList.push_back ({CheckShortEdges, &MeshChecker::checkShortEdges});
        m_checkList.push_back ({CheckTCount, &MeshChecker::checkTCount});
        m_checkList.push_back ({CheckDuplicateAnnotations, &MeshChecker::checkAnnotations});
        m_checkList.push_back ({CheckComponents, &MeshChecker::checkComponents});
        m_checkList.push_back ({CheckBadlyFormedTriangles, &MeshChecker::checkBadlyFormedTriangles});
        m_checkList.push_back ({CheckPockets, &MeshChecker::checkPockets});
    }
    return m_checkList;
}

MeshChecker::~MeshChecker ()
{
    delete m_octtree;
}

FileResult MeshChecker::check ()
{
    QLocale const locale;
    FileResult ret (m_path);

    bool pass = true;
    for (const auto& c : std::as_const (m_checkList))
    {
        if (m_checks & c.first)
        {
            auto res = (this->*c.second) ();
            pass &= res.pass ();
            ret.checkResults ().push_back (res);
            if (!res.pass () || res.badCount () >= 0)
            {
                const auto& name = checkName (c.first);
            }
        }
    }
    ret.setPass (pass);
    return ret;
}

HalfEdgesPtr MeshChecker::getEdges ()
{
    return m_edges;
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
                if (badCount < m_maxMessages)
                {
                    str += QStringLiteral ("    %1 referenced by half edges only %2 times\n").arg (fmtName (v)).arg (count);
                }
                badCount++;
            }
        }
    }
    if (badCount)
    {
        str.push_front (QStringLiteral ("  %1 low ref vertices found\n").arg (badCount));

        if (badCount > m_maxMessages)
        {
            str.push_back (QStringLiteral ("    ...\n"));
        }
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

CheckResult MeshChecker::checkOpenEdges ()
{
    QString str;

    auto edges = getEdges ();
    int badCount = 0;
    edges->matchHalfEdges ();

    for (const auto& e : edges->openEdges ())
    {
        if (badCount < m_maxMessages)
        {
            str += QStringLiteral ("    T: %1, Edge: %2 -> %3\n").arg (fmtName (e->triangle ()), fmtName (e->v1 ()), fmtName (e->v2 ()));
        }
        else if (badCount == m_maxMessages)
        {
            str += "    ...\n";
        }
        badCount++;
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

    int idx = -1;
    for (const auto& hole : std::as_const (holes))
    {
        idx++;
        if (idx < m_maxMessages)
        {
            auto area = hole->area ();
            r += QStringLiteral ("    Hole: %1 (%2sq)\n").arg (fmtName (hole)).arg (area);
        }
        else if (idx == m_maxMessages)
        {
            r += QStringLiteral ("    ...\n");
            break;
        }
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
    QString r;
    int badCount = 0;

    QHash<TriangleKey, int> thash;

    for (auto i = 0; i < count; i++)
    {
        const auto t = m_mesh->at (i);

        auto candidates = m_octtree->find (t->box ());
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
                    r += QStringLiteral ("    T: %1 and T: %2\n").arg (fmtName (t), fmtName (tt));
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

    int badCount = 0;
    int warnCount = 0;
    int foundShortEdge = 0;
    int foundNullEdge = 0;
    double smallest2 = INF;

    for (const auto& t : *m_mesh)
    {
        for (int ee = 0; ee < 3; ee++)
        {
            const auto& edge = t->halfEdge (ee);

            if (edge->v1 () == edge->v2 ())
            {
                foundNullEdge++;
                badCount++;
                ret += QStringLiteral ("    Null edge (repeating vertex): ") + edge->v1 ()->toString () + QStringLiteral ("\n");
            }
            auto mag2 = edge->magnitude2 ();
            if (mag2 <= Constants::minEdge2)
            {
                foundShortEdge++;

                if (warnCount < m_maxMessages)
                {
                    warnCount++;
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
                else if (warnCount == m_maxMessages)
                {
                    ret += "    ...\n";
                    warnCount++;
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

    return {CheckShortEdges, badCount == 0, ret, badCount};
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

    if (!m_allowOverusedEdges)
    {
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
                ret.push_back (QStringLiteral ("    edge: %1 -> %2\n").arg (fmtName (e->v1 ()), fmtName (e->v2 ())));
                for (const auto& match : values)
                {
                    ret.push_back (QStringLiteral ("      Used by T: %1\n").arg (fmtName (match->triangle ())));
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
    else
    {
#if 0
       Q_ASSERT (m_edges->testFlag (HalfEdges::EdgeMatched));

        for (const auto& e : m_edges->problemEdges2 ())
        {
            if (badCount < maxMessages)
            {
                ret.push_back (QStringLiteral ("    edge: %1 -> %2\n").arg (fmtName (e->v1 ()), fmtName (e->v2 ())));
                badCount++;
            }
            else if (badCount == maxMessages)
            {
                ret.push_back (QStringLiteral ("    ...\n"));
                badCount++;
            }
            else
            {
                badCount++;
            }
        }
#endif
    }
    if (badCount)
    {
        ret.push_front (QStringLiteral ("  %1 excess (single) half edges\n").arg (badCount));
        return {CheckOverusedHalfEdges, false, ret, badCount};
    }
    ret += QStringLiteral ("  No excess half edges\n");
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
        ret += QStringLiteral ("    %1 triangles, %2 vertices, %3 half edges\n").arg (locale.toString (m_mesh->tcount ()), locale.toString (vcount), locale.toString (ecount));
    }
    else
    {
        ret += QStringLiteral ("    %4 components, %1 triangles, %2 vertices, %3 half edges\n").arg (locale.toString (m_mesh->tcount ()), locale.toString (vcount), locale.toString (ecount)).arg (comps.count ());

        int idx = 0;
        for (const auto& comp : comps)
        {
            auto vcount = comp->vertexList ().count ();
            auto edges = HalfEdges::create (comp);
            auto ecount = comp->halfEdgeList ().count ();

            ret += QStringLiteral ("      comp %4: %1 triangles, %2 vertices, %3 half edges\n").arg (locale.toString (comp->tcount ()), locale.toString (vcount), locale.toString (ecount)).arg (idx++);

            if (comp->count () < m_maxMessages)
            {
                for (const auto& t : *comp)
                {
                    ret += QStringLiteral ("        T: %1\n").arg (fmtName (t));
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
    OctTree tree (m_meshBox);
    auto vs = m_mesh->vertexList ();
    for (const auto& v : std::as_const (vs))
    {
        auto res = tree.findOrAdd (v);
        if (res != v)
        {
            badCount++;
            ret += QStringLiteral ("    %1 and %2\n").arg (fmtName (v), fmtName (res));
        }
    }
    if (badCount)
    {
        ret.prepend (QStringLiteral ("  %1 duplicate vertices\n").arg (badCount));
        return {CheckDuplicateVertices, false, ret, badCount};
    }
    return {CheckDuplicateVertices, true, QStringLiteral ("  No duplicate vertices\n"), badCount};
}

namespace MeshCheckerNS {

struct Intersection
{
    Intersection (double t1, double t2) : m_edgeT (t1), m_segmentOfIntersectionT (t2) {}
    double m_edgeT;  // TODO: Don't seem to need this
    double m_segmentOfIntersectionT;
};

struct Intersections : public QList<Intersection*>
{
    ~Intersections () { qDeleteAll (*this); }
    void sortT2 ()
    {
        std::sort (begin (), end (), [] (const Intersection* a, const Intersection* b) -> bool {
            return a->m_segmentOfIntersectionT < b->m_segmentOfIntersectionT;
        });
    }
    double minT () const { return at (0)->m_segmentOfIntersectionT; }
    double maxT () const { return at (size () - 1)->m_segmentOfIntersectionT; }
    void dedup ()
    {
        for (int i = 0; i < count () - 1; i++)
        {
            if (std::abs (at (i)->m_segmentOfIntersectionT - at (i + 1)->m_segmentOfIntersectionT) < 0.001)
            {
                delete at (i);
                removeAt (i);
                i--;
            }
        }
    }
};
}  // namespace MeshCheckerNS

bool MeshChecker::checkNoneCoplanarOverlap (const TrianglePtr& target, const TrianglePtr& other, const SegmentPtr& segOfIntersection)
{
    using namespace MeshCheckerNS;
    Intersections tts;
    Intersections cts;

    auto m = Mesh::create ();
    m->push_back (target);
    m->push_back (other);

    auto inter = [&segOfIntersection] (const TrianglePtr& t) -> Intersections {
        Intersections ret;

        for (int e = 0; e < 3; e++)
        {
            //qDebug () << t->vertexAt (e)->magnitude(t->vertexAt ((e + 1) % 3));
            auto res = intersectionOfLines3D (t->vertexAt (e), t->vertexAt ((e + 1) % 3), segOfIntersection->start (), segOfIntersection->end ());
            if (std::isnan (res.t1) || std::isnan (res.t2))
            {
                // coplanar
                qDeleteAll (ret);
                ret.clear ();
                return ret;
            }
            if (0 <= res.t1 && res.t1 <= 1.0)
            {
                //auto v = m_vertexPool->vertex (res.vertexOfIntersection);
                ret.append (new Intersection{res.t1, res.t2});
            }
        }
        return ret;
    };

    auto targetIntersections = inter (target);
    if (targetIntersections.count () < 2)
    {
        return false;
    }
    auto otherIntersections = inter (other);
    if (otherIntersections.count () < 2)
    {
        return false;
    }

    targetIntersections.sortT2 ();
    otherIntersections.sortT2 ();
    targetIntersections.dedup ();
    otherIntersections.dedup ();
    if (otherIntersections.count () < 2 || targetIntersections.count () < 2)
    {
        return false;
    }

    // Is there an intersection?
    constexpr double E = 0.00000001;
    if (targetIntersections.maxT () <= otherIntersections.minT () + E || targetIntersections.minT () >= otherIntersections.maxT () - E)
    {
        return false;
    }
    return true;
}

CheckResult MeshChecker::checkOverlappingTriangles ()
{
    QString ret;
    QList<double> tts;
    QList<double> cts;
    bool hasOneOrMorePenetrations = false;

    struct OverlapPair
    {
        TrianglePtr first;
        TrianglePtr second;
        bool coplainar;
        double area{};
    };
    QList<OverlapPair> overlaps;

    double maxArea = 0;
    for (const auto& t : std::as_const (*m_mesh))
    {
        auto plane = t->plane ();
        if (plane.isValid ())
        {
            auto box = t->box ();
            auto candidates = m_octtree->find (box);

            for (const auto& candidate : std::as_const (candidates))
            {
                if (candidate != t && candidate->box ().intersects (box))
                {
                    auto p = candidate->plane ();

                    SegmentPtr segOfIntersection;

                    if (p.isValid ())
                    {
                        segOfIntersection = plane.intersection (p, 0.00000000000001);
                    }
                    bool intersect = false;
                    const CartesianPlane* bestPlane{};

                    auto test = [&bestPlane] (const HalfEdgePtr& he1, const HalfEdgePtr& he2) -> int {
                        Q_ASSERT (bestPlane);
                        auto res = intersectionOfLinesInPlane (*bestPlane, he1, he2);
                        if (res.significantCrossover ())
                        {
                            return 1;
                        }
                        return 0;
                    };

                    if (!segOfIntersection || !segOfIntersection->isValid ())
                    {
                        // No intersection of planes - must be parallel or the same plane, or same plane inverted
                        if (plane.equal (p))
                        {
                            if (plane.equalAndSameDirection (p))
                            {
                                // Coplanar Ts
                                const auto& c0 = candidate->halfEdge (0);
                                const auto& c1 = candidate->halfEdge (1);
                                const auto& c2 = candidate->halfEdge (2);
                                const auto& t0 = t->halfEdge (0);
                                const auto& t1 = t->halfEdge (1);
                                const auto& t2 = t->halfEdge (2);

                                bestPlane = &plane.bestPlane ().first;

                                intersect = test (c0, t0) || test (c0, t1) || test (c0, t2) || test (c1, t0) || test (c1, t1) || test (c1, t2) || test (c2, t0) || test (c2, t1) || test (c2, t2);

                                if (!intersect)
                                {
                                    auto res = t->containsWithDetails (candidate->centroid ());
                                    intersect = res == Triangle::TriangleContainsResult::Contained;
                                }
                            }
                        }
                    }
                    else
                    {
                        intersect = checkNoneCoplanarOverlap (t, candidate, segOfIntersection);
                    }

                    if (intersect)
                    {
                        if (t->id () < candidate->id ())
                        {
                            overlaps.push_back ({t, candidate, !segOfIntersection});
                        }
                        else
                        {
                            overlaps.push_back ({candidate, t, !segOfIntersection});
                        }
                    }
                }
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
            if (overlap.coplainar)
            {
                PolygonCorefiner pcr;
                try
                {
                    pcr.corefine (overlap.first->toPolygon (), overlap.second->toPolygon ());

                    auto res = pcr.boolIntersection ();
                    if (!res.isEmpty ())
                    {
                        overlap.area = res.constFirst ().area ();
                        maxArea = std::max (maxArea, overlap.area);
                    }
                }
                catch (...)
                {
                }
            }
            else
            {
                hasOneOrMorePenetrations = true;
            }
        }

        std::sort (overlaps.begin (), overlaps.end (), [] (const auto& a, const auto& b) -> bool {
            return a.area > b.area;
        });

        int idx = -1;
        for (const auto& overlap : overlaps)
        {
            idx++;
            if (idx < m_maxMessages)
            {
                if (overlap.coplainar)
                {
                    ret.push_back (QStringLiteral ("    Overlap: %1 and %2 (%3sq)\n").arg (fmtName (overlap.first), fmtName (overlap.second)).arg (overlap.area));
                }
                else
                {
                    ret.push_back (QStringLiteral ("    Overlap: %1 and %2 (penetrated)\n").arg (fmtName (overlap.first), fmtName (overlap.second)));
                }
            }
            else if (idx == m_maxMessages)
            {
                ret.push_back (QStringLiteral ("    ...\n"));
            }
            if (m_callback)
            {
                m_callback (CheckOverlappingTriangles, m_mesh, overlap.first, overlap.second);
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
    return {CheckOverlappingTriangles, maxArea < 0.01 || hasOneOrMorePenetrations, ret, (int)overlaps.count ()};
}

CheckResult MeshChecker::checkUnviableTriangles ()
{
    QString ret;
    int badCount = 0;
    for (const auto& t : *m_mesh)
    {
        if (!t->isViable ())
        {
            if (badCount < m_maxMessages)
            {
                ret += QStringLiteral ("    T: %1\n").arg (fmtName (t));
            }
            if (badCount == m_maxMessages)
            {
                ret += QStringLiteral ("    ...\n");
            }
            badCount++;
        }
    }
    if (!badCount)
    {
        return {CheckUnviableTriangles, true, QStringLiteral ("  No unviable triangles\n"), badCount};
    }
    ret.push_front (QStringLiteral ("  %1 unviable (small) triangles\n").arg (badCount));
    return {CheckUnviableTriangles, true, ret, badCount};
}

CheckResult MeshChecker::checkFlatTriangles ()
{
    QString ret;
    int badCount = 0;
    for (const auto& t : *m_mesh)
    {
        if (t->isFlat ())
        {
            if (badCount < m_maxMessages)
            {
                badCount++;
                ret += QStringLiteral ("    T: %1\n").arg (fmtName (t));
            }
            else if (badCount == m_maxMessages)
            {
                ret += QStringLiteral ("    ...\n");
                badCount++;
            }
            else
            {
                badCount++;
            }
        }
    }
    if (badCount)
    {
        ret.push_front (QStringLiteral ("  %1 flat triangles\n").arg (badCount));
        return {CheckFlatTriangles, true, ret, badCount};
    }
    return {CheckFlatTriangles, true, QStringLiteral ("  No flat triangles\n"), badCount};
}

CheckResult MeshChecker::checkBadlyFormedTriangles ()
{
    QString ret;
    int badCount = 0;
    for (const auto& t : *m_mesh)
    {
        if (t->v1 () == t->v2 () || t->v2 () == t->v3 () || t->v3 () == t->v1 ())
        {
            badCount++;
            ret += QStringLiteral ("    T: %1\n").arg (fmtName (t));
        }
    }
    if (badCount)
    {
        ret.push_front (QStringLiteral ("  %1 badly formed triangles\n").arg (badCount));
        return {CheckBadlyFormedTriangles, false, ret, badCount};
    }
    return {CheckBadlyFormedTriangles, true, QStringLiteral ("  No badly formed triangles\n"), badCount};
}

CheckResult MeshChecker::checkTCount ()
{
    auto cnt = m_mesh->tcount ();
    return {CheckTCount, cnt > 0, cnt == 0 ? "  Empty model!\n" : "", cnt};
}

CheckResult MeshChecker::checkComponents ()
{
    auto cnt = (int)m_mesh->splitComponents ().count ();
    return {CheckComponents, cnt > 0, cnt == 0 ? "Empty" : "", cnt};
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
                if (badCount < m_maxMessages)
                {
                    ret += QStringLiteral ("    T: %1 & %2  (\"%3\")\n").arg (fmtName (t), fmtName (tt), t->annotation ());
                }
                else if (badCount == m_maxMessages)
                {
                    ret += QStringLiteral ("    ...\n");
                }
                badCount++;
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
        return {CheckDuplicateAnnotations, true, ret, badCount};
    }
    return {CheckDuplicateAnnotations, true, QStringLiteral ("  No duplicate triangle annotations\n"), badCount};
}

CheckResult MeshChecker::checkPockets ()
{
    m_edges->matchHalfEdges ();

    QString log;
    QTextStream ts (&log);
    int badCount = 0;
    QSet<size_t> notedHash;
    for (const auto& t : *m_mesh)
    {
        const auto& un = t->unitNormal ();
        if (un)
        {
            for (int e = 0; e < 3; e++)
            {
                auto edge = t->halfEdge (e);
                if (edge->pair ())
                {
                    auto t2 = edge->pair ()->triangle ();
                    auto un2 = t2->unitNormal ();
                    if (un2 && un2->magnitude2 (un) > 3.99)
                    {
                        auto hash = qHash (std::min (t->id (), t2->id ()), std::max (t->id (), t2->id ()));
                        if (notedHash.contains (hash))
                        {
                            break;
                        }
                        notedHash.insert (hash);
                        badCount++;
                        if (badCount < m_maxMessages)
                        {
                            ts << "    T: " << fmtName (t) << " - T: " << fmtName (edge->pair ()->triangle ()) << '\n';
                            break;
                        }
                    }
                }
            }
        }
    }
    if (badCount == 0)
    {
        return {CheckPockets, true, QStringLiteral ("  No pockets found\n"), badCount};
    }
    if (badCount >= m_maxMessages)
    {
        ts << "    ...\n";
    }
    log.push_front (QStringLiteral ("  Found %1 pockets\n").arg (badCount));
    return {CheckPockets, true, log, badCount};
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
    case CheckOverlappingTriangles:
        return QStringLiteral ("OverlappingTriangles");
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
        return QStringLiteral ("Components");
    case CheckBadlyFormedTriangles:
        return QStringLiteral ("BadlyFormedTriangles");
    case CheckPockets:
        return QStringLiteral ("Pockets");
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
    case CheckOverlappingTriangles:
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
        return QStringLiteral ("Component count");
    case CheckBadlyFormedTriangles:
        return QStringLiteral ("Check for triangles that use the same vertex more than once");
    case CheckPockets:
        return QStringLiteral ("Check for pocket folds that create flat pockets");
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
    case CheckComponents:
    case CheckBadlyFormedTriangles:
    case CheckDuplicateAnnotations:
    case CheckPockets:
        return false;

    case CheckTCount:
    case CheckHoles:
    case CheckDuplicateTriangles:
    case CheckReversedTriangles:
    case CheckDuplicateVertices:
    case CheckOpenEdges:
    case CheckOverlappingTriangles:
    case CheckOverusedHalfEdges:
    case CheckDeleted:
    case CheckVertexLowRefs:
        return true;

    default:
        Q_ASSERT (false);
        return true;
    }
}

void MeshChecker::setListLimit (int value)
{
    m_maxMessages = value < 0 ? 0x7fffffff : value;
}

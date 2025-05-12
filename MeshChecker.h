#ifndef MESHCHECKER_H
#define MESHCHECKER_H

#include <TriangleOctTree/TriangleOctTree.h>
#include <Types.h>

#include <QFuture>
#include <QString>

class MeshChecker
{
public:
    enum Checks {
        CheckNothing                = 0,
        CheckHoles                  = 1 << 0,
        CheckDuplicateTriangles     = 1 << 2,
        CheckShortEdges             = 1 << 3,
        CheckInfo                   = 1 << 4,
        CheckReversedTriangles      = 1 << 5,
        CheckDuplicateVertices      = 1 << 6,
        CheckOpenEdges              = 1 << 7,
        CheckHalfEdgeOverlap        = 1 << 8,
        CheckTriangleOverlap        = 1 << 9,
        CheckUnviableTriangles      = 1 << 10,
        CheckOverusedEdges          = 1 << 11,
        CheckFlatTriangles          = 1 << 12,
        CheckDeleted                = 1 << 13,

        MultiThread                 = 1 << 18,

        Default = CheckHoles | CheckDuplicateTriangles | CheckShortEdges | CheckInfo | CheckReversedTriangles | CheckDuplicateVertices | CheckOpenEdges | CheckTriangleOverlap | CheckUnviableTriangles | CheckOverusedEdges | CheckFlatTriangles | CheckDeleted,
        All = CheckHoles | CheckDuplicateTriangles | CheckShortEdges | CheckInfo | CheckReversedTriangles | CheckDuplicateVertices | CheckOpenEdges | CheckHalfEdgeOverlap | CheckTriangleOverlap | CheckUnviableTriangles | CheckOverusedEdges | CheckFlatTriangles | CheckDeleted,
    };

    MeshChecker (const MeshPtr& mesh, const QString& path);
    virtual ~MeshChecker ();
    bool check ();
    bool checkMultiThreaded ();
    void setCheckFlags (uint flags) { m_checks = flags; }
    QString checkName (MeshChecker::Checks check);
    QString summary () const;

private:
    MeshPtr m_mesh;
    uint m_checks = Default;
    HalfEdgesPtr getEdges ();
    HalfEdgesPtr m_edgesPtr;
    QString m_summary;
    QFuture<HalfEdgesPtr> m_edgesFuture;
    QFuture<TriangleOctTree*> m_octtreeFuture;
    QString m_path;

    class CheckRet
    {
    public:
        CheckRet (bool pass, const QString& result, int badCount) : first (pass), second (result), m_badCount(badCount) {}
        bool first;
        QString second;
        int m_badCount {};
    };

    CheckRet checkHoles ();
    CheckRet checkDuplicateTriangles ();
    CheckRet checkShortEdges ();
    CheckRet checkInfo ();
    CheckRet checkReversedTriangles ();
    CheckRet checkDuplicateVertices ();
    CheckRet checkOpenEdges ();
    CheckRet checkHalfEdgeOverlap ();
    CheckRet checkTriangleOverlap ();
    CheckRet checkUnviableTriangles ();
    CheckRet checkOverusedEdges ();
    CheckRet checkFlatTriangles ();
    CheckRet checkDeleted ();

    void report (const CheckRet& res);

public:
    typedef MeshChecker::CheckRet (MeshChecker::*CheckFn) ();

};

#endif  // MESHCHECKER_H

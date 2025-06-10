#ifndef MESHCHECKER_H
#define MESHCHECKER_H

#include <TriangleOctTree/TriangleOctTree.h>
#include <Types.h>

#include <QFuture>
#include <QString>

#include "CheckResult.h"

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
        CheckOverusedHalfEdges      = 1 << 11,
        CheckFlatTriangles          = 1 << 12,
        CheckDeleted                = 1 << 13,
        CheckVertexLowRefs          = 1 << 14,

        MultiThread                 = 1 << 18,

        Default = CheckHoles | CheckDuplicateTriangles | CheckShortEdges | CheckInfo | CheckReversedTriangles | CheckDuplicateVertices | CheckOpenEdges | CheckTriangleOverlap | CheckUnviableTriangles | CheckOverusedHalfEdges | CheckFlatTriangles | CheckDeleted | CheckVertexLowRefs,
        All = CheckHoles | CheckDuplicateTriangles | CheckShortEdges | CheckInfo | CheckReversedTriangles | CheckDuplicateVertices | CheckOpenEdges | CheckHalfEdgeOverlap | CheckTriangleOverlap | CheckUnviableTriangles | CheckOverusedHalfEdges | CheckFlatTriangles | CheckDeleted | CheckVertexLowRefs,
        Critical = CheckHoles | CheckDuplicateTriangles | CheckReversedTriangles | CheckDuplicateVertices | CheckOpenEdges | CheckTriangleOverlap | CheckOverusedHalfEdges | CheckDeleted | CheckVertexLowRefs,
    };

    typedef CheckResult (MeshChecker::*CheckFn) ();
    typedef QList<QPair<MeshChecker::Checks, MeshChecker::CheckFn>> CheckList;

    static const CheckList& checkList ();
    static QString checkName (MeshChecker::Checks check);
    static QString optionName (MeshChecker::Checks check);
    static QString description (MeshChecker::Checks check);

    MeshChecker (const MeshPtr& mesh, const QString& path);
    virtual ~MeshChecker ();
    bool check ();
    bool checkMultiThreaded ();
    void setCheckFlags (uint flags) { m_checks = flags; }
    QString summary () const;
    QString path () const { return m_path; }

private:
    MeshPtr m_mesh;
    uint m_checks = Default;
    HalfEdgesPtr getEdges ();
    HalfEdgesPtr m_edgesPtr;
    QString m_summary;
    QFuture<HalfEdgesPtr> m_edgesFuture;
    QFuture<TriangleOctTree*> m_octtreeFuture;
    QString m_path;
    static CheckList m_checkList;

    CheckResult checkHoles ();
    CheckResult checkDuplicateTriangles ();
    CheckResult checkShortEdges ();
    CheckResult checkInfo ();
    CheckResult checkReversedTriangles ();
    CheckResult checkDuplicateVertices ();
    CheckResult checkOpenEdges ();
    CheckResult checkHalfEdgeOverlap ();
    CheckResult checkTriangleOverlap ();
    CheckResult checkUnviableTriangles ();
    CheckResult checkOverusedEdges ();
    CheckResult checkFlatTriangles ();
    CheckResult checkDeleted ();
    CheckResult checkVertexRefs ();

};

#endif  // MESHCHECKER_H

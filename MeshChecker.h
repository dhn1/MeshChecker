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

        MultiThread                 = 1 << 18,

        Default = CheckHoles | CheckDuplicateTriangles | CheckShortEdges | CheckInfo | CheckReversedTriangles | CheckDuplicateVertices | CheckOpenEdges | CheckTriangleOverlap | CheckUnviableTriangles | CheckOverusedEdges | CheckFlatTriangles,
        All = CheckHoles | CheckDuplicateTriangles | CheckShortEdges | CheckInfo | CheckReversedTriangles | CheckDuplicateVertices | CheckOpenEdges | CheckHalfEdgeOverlap | CheckTriangleOverlap | CheckUnviableTriangles | CheckOverusedEdges | CheckFlatTriangles,
    };

    MeshChecker (const MeshPtr& mesh);
    virtual ~MeshChecker ();
    bool check ();
    bool checkMultiThreaded ();
    void setCheckFlags (uint flags) { m_checks = flags; }

private:
    MeshPtr m_mesh;
    uint m_checks = Default;

    EdgesPtr getEdges ();
    EdgesPtr m_edgesPtr;

    QFuture<EdgesPtr> m_edgesFuture;
    QFuture<TriangleOctTree*> m_octtreeFuture;

    QPair<bool, QString> checkHoles ();
    QPair<bool, QString> checkDuplicateTriangles ();
    QPair<bool, QString> checkShortEdges ();
    QPair<bool, QString> checkInfo ();
    QPair<bool, QString> checkReversedTriangles ();
    QPair<bool, QString> checkDuplicateVertices ();
    QPair<bool, QString> checkOpenEdges ();
    QPair<bool, QString> checkHalfEdgeOverlap ();
    QPair<bool, QString> checkTriangleOverlap ();
    QPair<bool, QString> checkUnviableTriangles ();
    QPair<bool, QString> checkOverusedEdges ();
    QPair<bool, QString> checkFlatTriangles ();

    void report (QPair<bool, QString>& res);
};

#endif  // MESHCHECKER_H

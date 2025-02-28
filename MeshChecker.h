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
        CheckNothing = 0,
        CheckHoles = 1,
        CheckDuplicateTriangles = 2,
        CheckShortEdges = 8,
        ShowInfo = 16,
        CheckReversedTriangles = 32,
        CheckDuplicateVertices = 64,
        CheckOpenEdges = 128,
        CheckHalfEdgeOverlap = 256,
        CheckTriangleOverlap = 512,

        quiet = 2 << 16,
        verbose = 2 << 17,

        Default = CheckHoles | CheckDuplicateTriangles | CheckShortEdges | ShowInfo | CheckReversedTriangles | CheckDuplicateVertices | CheckOpenEdges | CheckHalfEdgeOverlap | CheckTriangleOverlap,
        All = CheckHoles | CheckDuplicateTriangles | CheckShortEdges | ShowInfo | CheckReversedTriangles | CheckDuplicateVertices | CheckOpenEdges | CheckHalfEdgeOverlap | CheckTriangleOverlap,
    };
    MeshChecker (const MeshPtr& mesh);
    virtual ~MeshChecker ();
    bool check ();
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
    QPair<bool, QString> showInfo ();
    QPair<bool, QString> checkReversedTriangles ();
    QPair<bool, QString> checkDuplicateVertices ();
    QPair<bool, QString> checkOpenEdges ();
    QPair<bool, QString> checkHalfEdgeOverlap ();
    QPair<bool, QString> checkTriangleOverlap ();
    QString vname (const VertexPtr& v);
};

#endif  // MESHCHECKER_H

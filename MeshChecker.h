#ifndef MESHCHECKER_H
#define MESHCHECKER_H

#include <TriangleOctTree/TriangleOctTree.h>
#include <Types.h>

#include <QString>

class MeshChecker
{
public:
    enum Checks {
        CheckNothing = 0,
        CheckHoles = 1,
        CheckDuplicateTriangles = 2,
        DrawHoles = 4,
        CheckShortEdges = 8,
        ShowInfo = 16,
        CheckReversedTriangles = 32,
        CheckDuplicateVertices = 64,
        CheckOpenEdges = 128,
        CheckHalfEdgeOverlap = 256,
        CheckTriangleOverlap = 512,

        Default = CheckHoles | CheckDuplicateTriangles | CheckShortEdges | ShowInfo | CheckReversedTriangles | CheckDuplicateVertices | CheckOpenEdges | CheckHalfEdgeOverlap | CheckTriangleOverlap,
        All = CheckHoles | CheckDuplicateTriangles | CheckShortEdges | ShowInfo | CheckReversedTriangles | CheckDuplicateVertices | CheckOpenEdges | CheckHalfEdgeOverlap | CheckTriangleOverlap,
    };
    MeshChecker (const MeshPtr& mesh);
    virtual ~MeshChecker ();
    bool check ();
    void setCheckFlag (uint flag) { m_checks |= flag; }
    void setCheckFlags (uint flags) { m_checks = flags; }

protected:
    virtual void report (const QString& str) = 0;
    virtual void verbose (const QString& str) = 0;

private:
    MeshPtr m_mesh;
    uint m_checks = Default;
    EdgesPtr m_edges;
    TriangleOctTree m_ttree;

    bool checkHoles ();
    bool checkDuplicateTriangles ();
    bool checkShortEdges ();
    void showInfo ();
    bool checkReversedTriangles();
    bool checkDuplicateVertices ();
    bool checkOpenEdges ();
    bool checkHalfEdgeOverlap ();
    bool checkTriangleOverlap ();
};

#endif  // MESHCHECKER_H

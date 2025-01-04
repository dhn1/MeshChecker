#ifndef MESHCHECKER_H
#define MESHCHECKER_H

#include <QString>

#include "Types.h"

class MeshChecker
{
public:
    enum Checks {
        CheckHoles = 1,
        CheckDuplicateTriangles = 2,
        DrawHoles = 4,
        CheckShortEdges = 8,
        ShowInfo = 16,
        CheckReversedTriangles = 32,
        CheckDuplicateVertices = 64,

        Default = CheckHoles | CheckDuplicateTriangles | CheckShortEdges | ShowInfo | CheckReversedTriangles | CheckDuplicateVertices
    };
    MeshChecker (const MeshPtr& mesh);
    virtual ~MeshChecker ();
    bool check ();
    void setCheckFlag (uint flag) { m_checks |= flag; }

protected:
    virtual void report (const QString& str) = 0;
    virtual void verbose (const QString& str) = 0;

private:
    MeshPtr m_mesh;
    uint m_checks = Default;
    EdgesPtr m_edges;

    bool checkHoles ();
    bool checkDuplicateTriangles ();
    bool checkShortEdges ();
    void showInfo ();
    bool checkReversedTriangles();
    bool checkDuplicateVertices ();
    bool checkOpenEdges ();
};

#endif  // MESHCHECKER_H

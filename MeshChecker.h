#ifndef MESHCHECKER_H
#define MESHCHECKER_H

#include <TriangleOctTree/TriangleOctTree.h>
#include <Types.h>

#include <QFuture>
#include <QString>

#include "CheckResult.h"
#include "FileResult.h"

class MeshChecker
{
public:


    typedef CheckResult (MeshChecker::*CheckFn) ();
    typedef QList<QPair<Checks, MeshChecker::CheckFn>> CheckList;

    static const CheckList& checkList ();
    static QString checkName (Checks check);
    static QString optionName (Checks check);
    static QString description (Checks check);

    MeshChecker (const MeshPtr& mesh, const QString& path);

    virtual ~MeshChecker ();
    FileResult check ();
    //bool checkMultiThreaded ();
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
    CheckResult checkOverusedHalfEdges ();
    CheckResult checkFlatTriangles ();
    CheckResult checkDeleted ();
    CheckResult checkVertexRefs ();

};

#endif  // MESHCHECKER_H

#ifndef MESHCHECKER_H
#define MESHCHECKER_H

#include <HalfEdges/Hole.h>
#include <TriangleOctTree/TriangleOctTree.h>
#include <Types.h>

#include <QFuture>
#include <QString>

#include "CheckResult.h"
#include "FileResult.h"

class MeshChecker
{
    friend class Tester;

public:
    typedef void (*CallbackFn) (Checks check, const MeshPtr&, const TrianglePtr&, const TrianglePtr&);

    typedef CheckResult (MeshChecker::*CheckFn) ();
    typedef QList<QPair<Checks, MeshChecker::CheckFn>> CheckList;

    static const CheckList& checkList ();
    static QString checkName (Checks check);
    static QString optionName (Checks check);
    static QString description (Checks check);
    static bool failable (Checks check);

    MeshChecker ();
    MeshChecker (const MeshPtr& mesh, const QString& path = {});
    virtual ~MeshChecker ();
    FileResult check ();
    void setCheckFlags (uint flags) { m_checks = flags; }
    QString path () const { return m_path; }
    CallbackFn callback () const;
    void setCallback (CallbackFn newCallback);
    void setData (const MeshPtr& mesh, const QString& path);

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
    CheckResult checkTCount ();
    CheckResult checkAnnotations ();
    CheckResult checkComponents ();
    CheckResult checkBadlyFormedTriangles ();
    CheckResult checkPockets ();

    static void setViewerHyperlinks (bool newViewerHyperlinks) { m_viewerHyperlinks = newViewerHyperlinks; }
    static void setAllowOverusedEdges (bool newAllowOverusedEdges) { m_allowOverusedEdges = newAllowOverusedEdges; }

private:
    MeshPtr m_mesh;
    uint m_checks = Default;
    HalfEdgesPtr getEdges ();
    HalfEdgesPtr m_edgesPtr;
    HalfEdgesPtr m_edges;
    TriangleOctTree* m_octtree{};
    QString m_path;
    CallbackFn m_callback{};
    Box m_meshBox;
    static bool m_allowOverusedEdges;

    static bool m_viewerHyperlinks;
    static CheckList m_checkList;

    QString fmtName (const TrianglePtr& t);
    QString fmtName (const VertexPtr& t);
    QString fmtName (const HolePtr& hole);
    void genMetaData ();
};

#endif  // MESHCHECKER_H

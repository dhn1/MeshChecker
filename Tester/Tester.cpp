#include <Document.h>
#include <DocumentNethers.h>
#include <LibMeshCheckerVersion.h>
#include <Matrix.h>
#include <Mesh.h>
#include <Plane.h>
#include <Triangle.h>
#include <libSculptVersion.h>
#include <test.h>

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QGuiApplication>

#include "MeshChecker.h"

static TestRes overlap01 ()
{
    // No overlap - coplanar

    auto target = Triangle::createIsosceles (50);
    auto other = Triangle::createIsosceles (50);
    other = other->transformed (Matrix::translate (0, 0, 10));

    auto mesh = Mesh::create ();
    mesh->push_back (target);
    mesh->push_back (other);

    MeshChecker mc (mesh);
    auto res = mc.checkTriangleOverlap ();

    VERIFY (res.m_badCount == 0)

    return Passed;
}
REGISTER_TEST (overlap01)

static TestRes overlap02 ()
{
    // No overlap - non-coplanar

    auto target = Triangle::createIsosceles (50);
    auto other = Triangle::createIsosceles (50);
    other = other->transformed (Matrix::translate (100, 0, 10));
    other = other->transformed (Matrix::rotateAroundY (degreesToRadians (30)));

    auto mesh = Mesh::create ();
    mesh->push_back (target);
    mesh->push_back (other);
    Document::write (mesh, outputFileName (gTestName, "nethers"));

    MeshChecker mc (mesh);
    auto res = mc.checkTriangleOverlap ();

    VERIFY (res.m_badCount == 0)

    return Passed;
}
REGISTER_TEST (overlap02)

static TestRes overlap03 ()
{
    // No overlap - share an edge with same unit normal - coplanar

    auto target = Triangle::createIsosceles (50);
    auto other = Triangle::createIsosceles (50);

    auto diff = other->v1 () - target->v2 ();
    other = other->transformed (Matrix::translate (diff));

    auto vp = VertexPool::create ();
    vp->update (target);
    vp->update (other);

    auto mesh = Mesh::create ();
    mesh->push_back (target);
    mesh->push_back (other);
    Document::write (mesh, outputFileName (gTestName, "nethers"));

    MeshChecker mc (mesh);
    auto res = mc.checkTriangleOverlap ();

    VERIFY (res.m_badCount == 0)

    return Passed;
}
REGISTER_TEST (overlap03)

static TestRes pentratingT ()
{
    // Piercing overlap
    // Overlap - non-coplanar

    auto target = Triangle::createIsosceles (50);
    auto other = Triangle::createIsosceles (50);
    other = other->transformed (Matrix::translate (55, 10, 10));
    other = other->transformed (Matrix::rotateAroundY (degreesToRadians (30)));

    auto mesh = Mesh::create ();
    mesh->push_back (target);
    mesh->push_back (other);
    Document::write (mesh, outputFileName (gTestName, "nethers"));

    MeshChecker mc (mesh);
    auto res = mc.checkTriangleOverlap ();

    VERIFY (res.m_badCount == 1)

    return Passed;
}
REGISTER_TEST (pentratingT)

static TestRes overlap05 ()
{
    // partial overlap along an edge

    auto target = Triangle::createIsosceles (50);
    auto other = Triangle::createIsosceles (50);

    auto diff = (other->v1 () - target->v2 ()) * 0.5;
    other = other->transformed (Matrix::translate (diff));

    auto vp = VertexPool::create ();
    vp->update (target);
    vp->update (other);

    auto mesh = Mesh::create ();
    mesh->push_back (target);
    mesh->push_back (other);
    Document::write (mesh, outputFileName (gTestName, "nethers"));

    MeshChecker mc (mesh);
    auto res = mc.checkTriangleOverlap ();

    VERIFY (res.m_badCount == 1)

    return Passed;
}
REGISTER_TEST (overlap05)

static TestRes overlap06 ()
{
    // partial overlap along an edge, but not coplanar

    auto target = Triangle::createIsosceles (50);
    auto other = Triangle::createIsosceles (50);

    auto diff = (other->v1 () - target->v2 ()) * 0.5;
    other = other->transformed (Matrix::translate (diff));
    other->replaceVertex (2, Vertex::create (other->vertexAt (2)->x (), other->vertexAt (2)->y (), other->vertexAt (2)->z () + 1));

    auto vp = VertexPool::create ();
    vp->update (target);
    vp->update (other);

    auto mesh = Mesh::create ();
    mesh->push_back (target);
    mesh->push_back (other);
    Document::write (mesh, outputFileName (gTestName, "nethers"));

    MeshChecker mc (mesh);
    auto res = mc.checkTriangleOverlap ();

    VERIFY (res.m_badCount == 0)

    return Passed;
}
REGISTER_TEST (overlap06)

static TestRes sharedEdgeNoneOverlap ()
{
    // shared v, slightly different planes
    auto target = Triangle::createIsosceles (50);
    auto other = Triangle::createIsosceles (50);

    other->replaceVertex (2, Vertex::create (other->vertexAt (2)->x (), other->vertexAt (2)->y (), other->vertexAt (2)->z () + 1));

    auto vp = VertexPool::create ();
    vp->update (target);
    vp->update (other);

    auto mesh = Mesh::create ();
    mesh->push_back (target);
    mesh->push_back (other);
    Document::write (mesh, outputFileName (gTestName, "nethers"));

    MeshChecker mc (mesh);
    auto res = mc.checkTriangleOverlap ();

    VERIFY (res.m_badCount == 0)

    return Passed;
}
REGISTER_TEST (sharedEdgeNoneOverlap)

static TestRes duplicates ()
{
    auto target = Triangle::createIsosceles (50);
    auto other = Triangle::createIsosceles (50);

    //other->replaceVertex(2, Vertex::create (other->vertexAt(2)->x(),other->vertexAt(2)->y(),other->vertexAt(2)->z() + 1));

    auto vp = VertexPool::create ();
    vp->update (target);
    vp->update (other);

    auto mesh = Mesh::create ();
    mesh->push_back (target);
    mesh->push_back (other);
    Document::write (mesh, outputFileName (gTestName, "nethers"));

    MeshChecker mc (mesh);
    auto res = mc.checkTriangleOverlap ();

    VERIFY (res.m_badCount == 1)

    return Passed;
}
REGISTER_TEST (duplicates)

static TestRes sharedEdgeOverlappingContained ()
{
    auto target = Triangle::createIsosceles (50);
    auto other = Triangle::createIsosceles (50);

    other->replaceVertex (2, other->centroid ());

    auto vp = VertexPool::create ();
    vp->update (target);
    vp->update (other);

    auto mesh = Mesh::create ();
    mesh->push_back (target);
    mesh->push_back (other);
    Document::write (mesh, outputFileName (gTestName, "nethers"));

    MeshChecker mc (mesh);
    auto res = mc.checkTriangleOverlap ();

    VERIFY (res.m_badCount == 1)

    return Passed;
}
REGISTER_TEST (sharedEdgeOverlappingContained)

static TestRes sharedEdgeOverlappingContaning ()
{
    auto target = Triangle::createIsosceles (50);
    auto other = Triangle::createIsosceles (50);

    auto seg = Segment (other->centroid (), other->vertexAt (2));
    other->replaceVertex (2, seg.forT (2));

    auto vp = VertexPool::create ();
    vp->update (target);
    vp->update (other);

    auto mesh = Mesh::create ();
    mesh->push_back (target);
    mesh->push_back (other);
    Document::write (mesh, outputFileName (gTestName, "nethers"));

    MeshChecker mc (mesh);
    auto res = mc.checkTriangleOverlap ();

    VERIFY (res.m_badCount == 1)

    return Passed;
}
REGISTER_TEST (sharedEdgeOverlappingContaning)

static TestRes useCase01 ()
{
    auto target = Triangle::create (Vertex::create (2.2936608791351318359, 6.1458978652954101563, 2.5), Vertex::create (0, 0, 2.5), Vertex::create (2.8679175376892089844, 5.0188555717468261719, 2.5), QStringLiteral ("TT17"), 0);
    auto other = Triangle::create (
        Vertex::create (3.1458981037139892578, 4.4732885360717773438, 0.5), Vertex::create (2.8679175376892089844, 5.0188555717468261719, 2.5), Vertex::create (2.9347445964813232422, 4.8877000808715820313, 2.5), QStringLiteral ("TT122"), 0);

    auto vp = VertexPool::create ();
    vp->update (target);
    vp->update (other);

    auto mesh = Mesh::create ();
    mesh->push_back (target);
    mesh->push_back (other);
    Document::write (mesh, outputFileName (gTestName, "nethers"));

    MeshChecker mc (mesh);
    auto res = mc.checkTriangleOverlap ();

    VERIFY (res.m_badCount == 0)

    return Passed;
}
REGISTER_TEST (useCase01)

static TestRes useCase02 ()
{
    auto target = Triangle::create (Vertex::create (8, 14, 0.5), Vertex::create (9.8541021347045898438, 13.706338882446289063, 2.5), Vertex::create (8.3708200454711914063, 13.941267967224121094, 2.5), QStringLiteral ("TT66"), 0);
    auto other = Triangle::create (Vertex::create (8, 14, 0.5), Vertex::create (8.3708200454711914063, 13.941267967224121094, 2.5), Vertex::create (8, 14, 2.5), QStringLiteral ("TT65"), 0);

    auto vp = VertexPool::create ();
    vp->update (target);
    vp->update (other);

    auto mesh = Mesh::create ();
    mesh->push_back (target);
    mesh->push_back (other);
    Document::write (mesh, outputFileName (gTestName, "nethers"));

    MeshChecker mc (mesh);
    auto res = mc.checkTriangleOverlap ();

    VERIFY (res.m_badCount == 0)

    return Passed;
}
REGISTER_TEST (useCase02)

static TestRes useCase03 ()
{
    auto target = Triangle::create (
        Vertex::create (0.39825222238099999839, 17.412785682100000884, -21), Vertex::create (-3.1186326712599998778, 14.056084250499999655, -21), Vertex::create (-3.7335426980599999425, 13.933771041400000001, -21), QStringLiteral ("TT42679"), 64);
    auto other = Triangle::create (
        Vertex::create (-3.7335426980599999425, 13.933771041400000001, -19), Vertex::create (-3.1186326712599998778, 14.056084250499999655, -21), Vertex::create (-3.1147861431099999052, 14.056849372600000336, -21), QStringLiteral ("TT43711"), 64);

    auto vp = VertexPool::create ();
    vp->update (target);
    vp->update (other);

    auto mesh = Mesh::create ();
    mesh->push_back (target);
    mesh->push_back (other);
    Document::write (mesh, outputFileName (gTestName, "nethers"));

    MeshChecker mc (mesh);
    auto res = mc.checkTriangleOverlap ();

    VERIFY (res.m_badCount == 0)

    return Passed;
}
REGISTER_TEST (useCase03)

static TestRes useCase04 ()
{
    auto target =
        Triangle::create (Vertex::create (-6.415164999259999945, 5.6080837660800000322, -18), Vertex::create (-6.1588704557899998093, 5.8913302244899998783, -18), Vertex::create (-6.214446746370000163, 5.8357539339200004136, -18), "TT70491", 64);
    auto other = Triangle::create (
        Vertex::create (-6.2988778737000004071, 6.7076236509099995686, -17.918788480900001758), Vertex::create (-6.214446746370000163, 5.8357539339200004136, -18), Vertex::create (-6.0503716433100001026, 5.9998290369799995858, -18), "TT45273", 64);

    auto vp = VertexPool::create ();
    vp->update (target);
    vp->update (other);

    auto mesh = Mesh::create ();
    mesh->push_back (target);
    mesh->push_back (other);
    Document::write (mesh, outputFileName (gTestName, "nethers"));

    MeshChecker mc (mesh);
    auto res = mc.checkTriangleOverlap ();

    VERIFY (res.m_badCount == 0)

    return Passed;
}
REGISTER_TEST (useCase04)

static TestRes useCase05 ()
{
    auto target = Triangle::create (Vertex::create (34.983099559400002931, 2.2009500682800000604, -9.635073482030000136), Vertex::create (35.569933053799999811, 2.2378705023099998428, -9.0798099947900006157),
        Vertex::create (35.640260967499997946, 2.182336575730000151e-15, -9.0798099947900006157), "TT12949", 64);
    auto other = Triangle::create (Vertex::create (35.052267201799999441, 2.1463323415699999308e-15, -9.635073482030000136), Vertex::create (35.640260967499997946, 2.182336575730000151e-15, -9.0798099947900006157),
        Vertex::create (35.569933053799999811, -2.2378705023099998428, -9.0798099947900006157), "TT12951", 64);

    auto vp = VertexPool::create ();
    vp->update (target);
    vp->update (other);

    auto mesh = Mesh::create ();
    mesh->push_back (target);
    mesh->push_back (other);
    Document::write (mesh, outputFileName (gTestName, "nethers"));

    MeshChecker mc (mesh);
    auto res = mc.checkTriangleOverlap ();

    VERIFY (res.m_badCount == 0)

    return Passed;
}
REGISTER_TEST (useCase05)

static TestRes useCase06 ()
{
    auto target = Triangle::create (Vertex::create (18.797366735400000692, 1.1826300734099999268, -15.923727499600000002), Vertex::create (19.825591414200001594, 1.2139673533299999681e-15, -15.620066621799999496),
        Vertex::create (19.786470138300000343, 1.2448591848699999129, -15.620066621799999496), "TT36149", 64);
    auto other = Triangle::create (Vertex::create (18.834532382799999084, 1.1532824897999999192e-15, -15.923727499600000002), Vertex::create (19.786470138300000343, -1.2448591848699999129, -15.620066621799999496),
        Vertex::create (19.825591414200001594, 1.2139673533299999681e-15, -15.620066621799999496), "TT36151", 64);

    auto vp = VertexPool::create ();
    vp->update (target);
    vp->update (other);

    auto mesh = Mesh::create ();
    mesh->push_back (target);
    mesh->push_back (other);
    Document::write (mesh, outputFileName (gTestName, "nethers"));

    MeshChecker mc (mesh);
    auto res = mc.checkTriangleOverlap ();

    VERIFY (res.m_badCount == 0)

    return Passed;
}
REGISTER_TEST (useCase06)

static TestRes useCase07 ()
{
    auto target = Triangle::create (Vertex::create (17.42475366749999921, 0.017424753667500000875, -19.004668948700000897), Vertex::create (17.417528385899998966, 0.24733694153899998813, -19.066273733700001003),
        Vertex::create (17.425301262400001434, 1.0669919707600000829e-15, -18), "TT43942", 1088);
    auto other = Triangle::create (
        Vertex::create (17.425301262400001434, 1.0669919707600000829e-15, -18), Vertex::create (17.412785682100000884, -0.39825222238099999839, -18), Vertex::create (17.425301262400001434, 4.4088823393800002323e-16, -19), "TT111919", 64);

    auto vp = VertexPool::create ();
    vp->update (target);
    vp->update (other);

    auto mesh = Mesh::create ();
    mesh->push_back (target);
    mesh->push_back (other);
    Document::write (mesh, outputFileName (gTestName, "nethers"));

    MeshChecker mc (mesh);
    auto res = mc.checkTriangleOverlap ();

    VERIFY (res.m_badCount == 0)

    return Passed;
}
REGISTER_TEST (useCase07)

static TestRes useCase08 ()
{
    auto target = Triangle::create (Vertex::create (44.075474737199996866, 79.067209756799996967, 2), Vertex::create (0.9442936788789999536, 15.640481640799999141, 2), Vertex::create (0.633628940779000005, 12.819595897199999257, 2), "TT262", 0);
    auto other = Triangle::create (Vertex::create (0.9442936788789999536, 15.640481640799999141, 2), Vertex::create (0.633628940779000005, 12.819595897199999257, 39.5), Vertex::create (0.633628940779000005, 12.819595897199999257, 2), "TT159", 0);

    auto vp = VertexPool::create ();
    vp->update (target);
    vp->update (other);

    auto mesh = Mesh::create ();
    mesh->push_back (target);
    mesh->push_back (other);
    Document::write (mesh, outputFileName (gTestName, "nethers"));

    MeshChecker mc (mesh);
    auto res = mc.checkTriangleOverlap ();

    VERIFY (res.m_badCount == 0)

    return Passed;
}
REGISTER_TEST (useCase08)

static TestRes useCase09 ()
{
    auto target =
        Triangle::create (Vertex::create (3.3512096405029296875, 7.8341774940490722656, -18), Vertex::create (3.8266692161560058594, 7.613307952880859375, -18), Vertex::create (3.7787721157073974609, 7.6377129554748535156, -18), "TT21370", 0);
    auto other = Triangle::create (
        Vertex::create (4.43286895751953125, 8.063358306884765625, -17.918788909912109375), Vertex::create (3.6297621726989746094, 7.7136373519897460938, -18), Vertex::create (3.8365087509155273438, 7.6082944869995117188, -18), "TT20013", 0);

    auto vp = VertexPool::create ();
    vp->update (target);
    vp->update (other);

    auto mesh = Mesh::create ();
    mesh->push_back (target);
    mesh->push_back (other);
    Document::write (mesh, outputFileName (gTestName, "nethers"));

    MeshChecker mc (mesh);
    auto res = mc.checkTriangleOverlap ();

    VERIFY (res.m_badCount == 0)

    return Passed;
}
REGISTER_TEST (useCase09)

static TestRes sharedEdge01 ()
{
    auto v1 = Vertex::create (0, 100);
    auto v2 = Vertex::create (0, -100);
    auto v3 = Vertex::create (-100, 0);
    auto v4 = Vertex::create (100, 0);
    v1->setAnnotation ("V1");
    v2->setAnnotation ("V2");
    v3->setAnnotation ("V3");
    v4->setAnnotation ("V4");

    auto target = Triangle::create (v1, v4, v2);
    target->setEdgeAnnotation (0, "E0");
    target->setEdgeAnnotation (1, "E1");
    target->setEdgeAnnotation (2, "E2");

    auto other = Triangle::create (v1, v2, v3);

    auto vp = VertexPool::create ();
    vp->update (target);
    vp->update (other);

    auto mesh = Mesh::create ();
    mesh->colourise ();
    mesh->push_back (target);
    mesh->push_back (other);
    Document::write (mesh, outputFileName (gTestName, "nethers"));

    MeshChecker mc (mesh);
    auto res = mc.checkTriangleOverlap ();

    VERIFY (res.m_badCount == 0)

    target->reverse ();
    res = mc.checkTriangleOverlap ();

    VERIFY (res.m_badCount == 0)

    return Passed;
}
REGISTER_TEST (sharedEdge01)

static TestRes sharedEdge02 ()
{
    auto v1 = Vertex::create (0, 100);
    auto v2 = Vertex::create (0, -100);
    auto v3 = Vertex::create (0, 0, -100);
    auto v4 = Vertex::create (100, 0);
    v1->setAnnotation ("V1");
    v2->setAnnotation ("V2");
    v3->setAnnotation ("V3");
    v4->setAnnotation ("V4");

    auto target = Triangle::create (v1, v4, v2);
    target->setEdgeAnnotation (0, "E0");
    target->setEdgeAnnotation (1, "E1");
    target->setEdgeAnnotation (2, "E2");

    auto other = Triangle::create (v1, v2, v3);

    auto vp = VertexPool::create ();
    vp->update (target);
    vp->update (other);

    auto mesh = Mesh::create ();
    mesh->colourise ();
    mesh->push_back (target);
    mesh->push_back (other);
    Document::write (mesh, outputFileName (gTestName, "nethers"));

    MeshChecker mc (mesh);
    auto res = mc.checkTriangleOverlap ();

    VERIFY (res.m_badCount == 0)

    target->reverse ();
    res = mc.checkTriangleOverlap ();

    VERIFY (res.m_badCount == 0)

    return Passed;
}
REGISTER_TEST (sharedEdge02)

static TestRes sharedEdge03 ()
{
    auto v1 = Vertex::create (0, 100);
    auto v2 = Vertex::create (0, -100);
    auto v3 = Vertex::create (50, 0);
    auto v4 = Vertex::create (100, 0);

    auto target = Triangle::create (v1, v4, v2);
    auto other = Triangle::create (v1, v3, v2);

    auto vp = VertexPool::create ();
    vp->update (target);
    vp->update (other);

    auto mesh = Mesh::create ();
    mesh->colourise ();
    mesh->push_back (target);
    mesh->push_back (other);
    Document::write (mesh, outputFileName (gTestName, "nethers"));

    MeshChecker mc (mesh);
    auto res = mc.checkTriangleOverlap ();

    VERIFY (res.m_badCount == 1)

    target->reverse ();
    res = mc.checkTriangleOverlap ();

    VERIFY (res.m_badCount == 1)

    return Passed;
}
REGISTER_TEST (sharedEdge03)

static TestRes useCase10 ()
{
    auto t1 = Triangle::create (Vertex::create (28.719999999999998863, 0.080000000000000001665, 0), Vertex::create (29.18973953169999902, 0.32000000000000000666, -2.2972823229100001186),
        Vertex::create (29.280000000000001137, 0.32000000000000000666, 0), QByteArrayLiteral ("t1"), 0);
    auto t2 = Triangle::create (Vertex::create (28.631465824799999353, 0.080000000000000001665, 2.2533452292999998079), Vertex::create (29.280000000000001137, 0.32000000000000000666, 0),
        Vertex::create (29.18973953169999902, 0.32000000000000000666, 2.2972823229100001186), QStringLiteral ("t2"), 0);

    auto vp = VertexPool::create ();
    vp->update (t1);
    vp->update (t2);

    auto mesh = Mesh::create ();
    mesh->colourise ();
    mesh->push_back (t1);
    mesh->push_back (t2);
    Document::write (mesh, outputFileName (gTestName, "nethers"));

    MeshChecker mc (mesh);
    auto res = mc.checkTriangleOverlap ();

    VERIFY (res.m_badCount == 0)

    mesh->push_back (mesh->takeFirst ());
    res = mc.checkTriangleOverlap ();

    VERIFY (res.m_badCount == 0)
    t1->reverse ();
    res = mc.checkTriangleOverlap ();

    VERIFY (res.m_badCount == 0)

    return Passed;
}
REGISTER_TEST (useCase10)

static TestRes useCase11 ()
{
    auto t1 = Triangle::create (
        Vertex::create (19.798989873200000034, 2.1107702762699998011, -19.798989873200000034), Vertex::create (21.288255852300000726, 100, -18.181888150599998966), Vertex::create (19.796096761200001168, 100, -19.796096761200001168), "t1", 0);

    auto t2 = Triangle::create (Vertex::create (21.213203435600000546, 100, -21.213203435600000546), Vertex::create (18.181888150599998966, 100, -21.288255852300000726), Vertex::create (19.796096761200001168, 100, -19.796096761200001168), "t2", 0);

    auto vp = VertexPool::create ();
    vp->update (t1);
    vp->update (t2);

    auto mesh = Mesh::create ();
    mesh->colourise ();
    mesh->push_back (t1);
    mesh->push_back (t2);
    Document::write (mesh, outputFileName (gTestName, "nethers"));

    MeshChecker mc (mesh);
    auto res = mc.checkTriangleOverlap ();

    VERIFY (res.m_badCount == 0)

    mesh->push_back (mesh->takeFirst ());
    res = mc.checkTriangleOverlap ();

    VERIFY (res.m_badCount == 0)

    t1->reverse ();
    res = mc.checkTriangleOverlap ();

    VERIFY (res.m_badCount == 0)

    return Passed;
}
REGISTER_TEST (useCase11)

static TestRes useCase12 ()
{
    auto t1 = Triangle::create (Vertex::create (5.0643400553599997593, 114.87999999999999545, 0.39857220629700002013), Vertex::create (4, 115, 0), Vertex::create (3.9876693349300000868, 115, 0.31383638291100002249), "", 0);

    auto t2 = Triangle::create (Vertex::create (5.0800000000000000711, 114.87999999999999545, 0), Vertex::create (3.9876693349300000868, 115, -0.31383638291100002249), Vertex::create (4, 115, 0), "", 0);

    auto vp = VertexPool::create ();
    vp->update (t1);
    vp->update (t2);

    auto mesh = Mesh::create ();
    mesh->colourise ();
    mesh->push_back (t1);
    mesh->push_back (t2);
    Document::write (mesh, outputFileName (gTestName, "nethers"));

    MeshChecker mc (mesh);
    auto res = mc.checkTriangleOverlap ();

    VERIFY (res.m_badCount == 0)

    mesh->push_back (mesh->takeFirst ());
    res = mc.checkTriangleOverlap ();

    VERIFY (res.m_badCount == 0)

    t1->reverse ();
    res = mc.checkTriangleOverlap ();
    VERIFY (res.m_badCount == 0)
    return Passed;
}
REGISTER_TEST (useCase12)

static TestRes useCase13     ()
{
    auto t1 = Triangle::create (Vertex::create (0, 40, 0), Vertex::create (19.999999999999996447, 34.641016151377549193, 40), Vertex::create (39.125904029352227553, -8.3164676327103652653, 40));
    auto t2 = Triangle::create (Vertex::create (-38.042260651806145688, 12.360679774997889169, 0), Vertex::create (-26.765224254354350819, 29.725793019095746672, 40), Vertex::create (19.999999999999996447, 34.641016151377549193, 40));
    auto vp = VertexPool::create ();

    vp->update (t1);
    vp->update (t2);

    auto mesh = Mesh::create ();
    mesh->colourise ();
    mesh->push_back (t1);
    mesh->push_back (t2);

    auto segOfIntersection = t1->plane().intersection(t2->plane());
    DocumentNethers doc (mesh);
    doc.add(Segment::create(segOfIntersection));
    doc.write (outputFileName (gTestName, "nethers"));

    MeshChecker mc (mesh);
    auto res = mc.checkTriangleOverlap ();

    VERIFY (res.m_badCount == 0)

    mesh->push_back (mesh->takeFirst ());
    res = mc.checkTriangleOverlap ();

    VERIFY (res.m_badCount == 0)

    t1->reverse ();
    res = mc.checkTriangleOverlap ();
    VERIFY (res.m_badCount == 0)
    return Passed;
}
REGISTER_TEST(useCase13)

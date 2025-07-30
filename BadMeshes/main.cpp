#include <Document.h>
#include <Matrix.h>
#include <Mesh.h>
#include <libSculptVersion.h>

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QGuiApplication>

static void generateDupTriangleMesh (const QString& path)
{
    const auto mesh = Mesh::createSphere (100, 10);
    auto t = mesh->at (1);
    mesh->push_back (t);
    t = mesh->at (20);
    auto t2 = Triangle::create (t->v2 (), t->v3 (), t->v1 ());
    mesh->push_back (t2);

    Document::write (mesh, path + "/2DupTs.nethers");
}

static void generateHolesMesh (const QString& path)
{
    const auto mesh = Mesh::createSphere (100, 10);
    mesh->removeAt(7);
    mesh->removeAt(123);

    Document::write (mesh, path + "/2Holes.nethers");
}

static void generateReversedTsMesh (const QString& path)
{
    const auto mesh = Mesh::createSphere (100, 10);
    mesh->at (8)->reverse ();
    mesh->at (133)->reverse ();

    Document::write (mesh, path + "/2reverses.nethers");
}

static void generateDupVsMesh (const QString& path)
{
    const auto mesh = Mesh::createSphere (100, 10);

    auto t =mesh->at(47);
    t->replaceVertex (0, t->vertexAt (0)->clone ());
    t->replaceVertex (1, t->vertexAt (1)->clone ());

    Document::write (mesh, path + "/2dupVs.nethers");
}

static void generateOverlappingTsCoplanar (const QString& path)
{
    const auto mesh = Mesh::createCube (100);

    auto t = mesh->at (2);
    mesh->push_back (t->scaledAboutCenter (0.75));

    Document::setExcludeDeleted (false);
    Document::write (mesh, path + "/overlapsCoplanar.nethers");
    Document::setExcludeDeleted (true);
}

static void generateOverlappingTs (const QString& path)
{
    const auto mesh = Mesh::createCube(100);

    auto t2 = mesh->at (7)->scaledAboutCenter(0.75);
    auto m = Matrix::rotateAroundZ (degreesToRadians (30));
    mesh->push_back (t2->transformed (m));

    Document::setExcludeDeleted (false);
    Document::write (mesh, path + "/overlaps.nethers");
    Document::setExcludeDeleted (true);
}

static void generateOverusedHalfEdges (const QString& path)
{
    const auto mesh = Mesh::createCube(100);
    const auto mesh2 = Mesh::createCube(100);
    mesh2->transform (Matrix::rotateAroundZ (degreesToRadians (45 + 90 + 90 + 45)));

    auto idx = 1;
    auto diff = mesh2->at(idx)->vertexAt(0) - mesh->at (idx)->vertexAt(1);
    mesh2->translate (diff->x (), diff->y (), 0.0);

    auto vp = VertexPool::create();
    vp->update(*mesh);
    vp->update(*mesh2);

    mesh->add (mesh2);
    Document::write (mesh, path + "/1overusedEdge.nethers");
}

static void generateDeletedT (const QString& path)
{
    const auto mesh = Mesh::createCube(100);
    mesh->at(10)->setFlag(Triangle::Delete);
    Document::write (mesh, path + "/1deletedT.nethers");
}

int main (int argc, char* argv[])
{
    QGuiApplication const a (argc, argv);

    QCoreApplication::setApplicationName (QStringLiteral ("BadMeshes"));
    QCoreApplication::setApplicationVersion (QStringLiteral (VERSION) + QStringLiteral (" (libSculpt ") + libSculptVersion () + QStringLiteral (")"));
    QCoreApplication::setOrganizationName (QStringLiteral ("Netherwood Industries"));

    QCommandLineParser parser;
    parser.setApplicationDescription (QStringLiteral ("Generate bad mesh files to test MeshChecker"));
    parser.addHelpOption ();
    parser.addVersionOption ();
    parser.addOption (QCommandLineOption (QStringList () << QStringLiteral ("output") << QStringLiteral ("o"), QStringLiteral ("output folder (default is /tmp)")));
    parser.process (a);

    QString outFolder ("/tmp");
    if (parser.isSet ("output"))
    {
        outFolder = parser.value ("output");
    }

    generateDupTriangleMesh (outFolder);
    generateHolesMesh (outFolder);
    generateReversedTsMesh (outFolder);
    generateDupVsMesh (outFolder);
    generateOverlappingTs (outFolder);
    generateOverlappingTsCoplanar (outFolder);
    generateOverusedHalfEdges (outFolder);
    generateDeletedT (outFolder);
}

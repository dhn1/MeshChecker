#include <Mesh.h>

#include <QCommandLineParser>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QGuiApplication>

#include "Document.h"
#include "MeshChecker.h"
#include "globals.h"

static QStringList suffixes{"nethers", "stl", "obj", "3mf"};

static bool ok = true;
static int flags = MeshChecker::CheckNothing;
QTextStream out (stdout);

static bool processFile (const QString& path)
{
    Triangle::resetID ();
    auto mesh = Document::readMesh (path);
    if (!mesh)
    {
        qDebug ().nospace ().noquote () << "Unable to open: \"" << path << "\"\n";
        return false;
    }
    if (!(flags & MeshChecker::Quiet))
    {
        out << path << '\n';
    }
    MeshChecker checker (mesh);

    checker.setCheckFlags (flags);

    bool ret;
    if (flags & MeshChecker::MultiThread)
    {
        ret = checker.checkMultiThreaded ();
    }
    else
    {
        ret = checker.check ();
    }
    ok &= ret;
    if (!(flags & MeshChecker::Quiet) && !ret)
    {
        out << "found errors in: " << path << "\n";
        out.flush ();
    }
    return ret;
}

static void files (const QString& path)
{
    QFileInfo const inf (path);
    if (!inf.exists ())
    {
        qDebug ().nospace ().noquote () << path << " does not exist";
    }
    if (inf.isFile () && suffixes.contains (inf.suffix ()))
    {
        processFile (path);
    }
    else if (inf.isDir ())
    {
        QDir const d (path);
        auto entries = d.entryInfoList (QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
        for (const auto& e : std::as_const (entries))
        {
            files (e.absoluteFilePath ());
        }
    }
}

int main (int argc, char** argv)
{
    QGuiApplication const a (argc, argv);

    QCoreApplication::setApplicationName (QStringLiteral ("MeshChecker"));
    QCoreApplication::setApplicationVersion (QStringLiteral (VERSION));
    QCoreApplication::setOrganizationName (QStringLiteral ("Netherwood Industries"));

    QCommandLineParser parser;
    parser.setApplicationDescription (QStringLiteral ("Check mesh for errors.\nNB, for 3mf files, only checks the main model (there can be more). Use 3mfsplitter app to work around this limit."));
    parser.addHelpOption ();
    parser.addVersionOption ();
    parser.addPositionalArgument (QStringLiteral ("mesh"), QStringLiteral ("3D file to examine (STL, 3MF or OBJ)"));
    parser.addOption (QCommandLineOption (QStringLiteral ("verbose"), QStringLiteral ("Show details of errors")));
    parser.addOption (QCommandLineOption (QStringList () << QStringLiteral ("multi-thread") << QStringLiteral ("mt"), QStringLiteral ("Multi threaded execution (experimental)")));
    parser.addOption (QCommandLineOption (QStringList () << QStringLiteral ("quiet") << QStringLiteral ("q"), QStringLiteral ("Only print names of incorrect files")));
    parser.addOption (QCommandLineOption (QStringList () << QStringLiteral ("ShowInfo"), QStringLiteral ("Show basic stats")));
    parser.addOption (QCommandLineOption (QStringList () << QStringLiteral ("CheckHoles"), QStringLiteral ("check for holes")));
    parser.addOption (QCommandLineOption (QStringList () << QStringLiteral ("CheckDuplicateTriangles"), QStringLiteral ("check for duplicate triangles")));
    parser.addOption (QCommandLineOption (QStringList () << QStringLiteral ("CheckShortEdges"), QStringLiteral ("check for short edges")));
    parser.addOption (QCommandLineOption (QStringList () << QStringLiteral ("CheckReversedTriangles"), QStringLiteral ("check for reversed triangles")));
    parser.addOption (QCommandLineOption (QStringList () << QStringLiteral ("CheckDuplicateVertices"), QStringLiteral ("check for duplicate vertices")));
    parser.addOption (QCommandLineOption (QStringList () << QStringLiteral ("CheckOpenEdges"), QStringLiteral ("check for open edges")));
    parser.addOption (QCommandLineOption (QStringList () << QStringLiteral ("CheckHalfEdgeOverlap"), QStringLiteral ("check for overlapping half edges")));
    parser.addOption (QCommandLineOption (QStringList () << QStringLiteral ("CheckTriangleOverlap"), QStringLiteral ("check for overlapping triangles")));
    parser.addOption (QCommandLineOption (QStringList () << QStringLiteral ("all") << QStringLiteral ("a"), QStringLiteral ("check for overlapping triangles")));

    parser.process (a);

    if (parser.isSet (QStringLiteral ("CheckHoles")))
    {
        flags |= MeshChecker::CheckHoles;
    }
    if (parser.isSet (QStringLiteral ("CheckDuplicateTriangles")))
    {
        flags |= MeshChecker::CheckDuplicateTriangles;
    }
    if (parser.isSet (QStringLiteral ("CheckShortEdges")))
    {
        flags |= MeshChecker::CheckShortEdges;
    }
    if (parser.isSet (QByteArrayLiteral ("ShowInfo")))
    {
        flags |= MeshChecker::CheckInfo;
    }
    if (parser.isSet (QStringLiteral ("CheckReversedTriangles")))
    {
        flags |= MeshChecker::CheckReversedTriangles;
    }
    if (parser.isSet (QStringLiteral ("CheckDuplicateVertices")))
    {
        flags |= MeshChecker::CheckDuplicateVertices;
    }
    if (parser.isSet (QStringLiteral ("CheckOpenEdges")))
    {
        flags |= MeshChecker::CheckOpenEdges;
    }
    if (parser.isSet (QStringLiteral ("CheckHalfEdgeOverlap")))
    {
        flags |= MeshChecker::CheckHalfEdgeOverlap;
    }
    if (parser.isSet (QStringLiteral ("CheckTriangleOverlap")))
    {
        flags |= MeshChecker::CheckTriangleOverlap;
    }
    if (parser.isSet (QStringLiteral ("all")))
    {
        flags |= MeshChecker::All;
    }
    if (parser.isSet (QStringLiteral ("ShowInfo")))
    {
        flags |= MeshChecker::CheckInfo;
    }
    if (flags == 0)
    {
        flags = MeshChecker::Default;
    }

    if (parser.isSet ("mt"))
    {
        flags |= MeshChecker::MultiThread;
    }
    if (parser.isSet (QStringLiteral ("verbose")))
    {
        flags |= MeshChecker::Verbose;
    }
    if (parser.isSet (QStringLiteral ("quiet")))
    {
        flags |= MeshChecker::Quiet;
    }

    if (parser.positionalArguments ().isEmpty ())
    {
        qDebug ().nospace ().noquote () << "Missing argument";
        parser.showHelp (100);
    }

    QElapsedTimer et;
    et.start ();
    files (parser.positionalArguments ().at (0));
    QLocale const locale;
    out << "\nTook " << locale.toString ((double)et.nsecsElapsed () / 1000000000.0) << " seconds" << '\n';
    return (ok ? 0 : 100);
}

#include <Mesh.h>

#include <QCommandLineParser>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QGuiApplication>

#include "Document.h"
#include "MeshChecker.h"
#include "globals.h"

Verbosity verbosity{Mute};

static QStringList suffixes{"nethers", "stl", "obj", "3mf"};

static bool ok = true;
static int flags = MeshChecker::CheckNothing;
static int fileCount = 0;
static int failCount = 0;

QTextStream out (stdout);

static bool processFile (const QString& path)
{
    fileCount++;
    Triangle::resetID ();
    auto mesh = Document::readMesh (path);
    if (!mesh)
    {
        qDebug ().nospace ().noquote () << "Unable to open: \"" << path << "\"\n";
        failCount++;
        return false;
    }

    if (verbosity & FileName)
    {
        out << path;
        if (verbosity & (Summary | Details))
        {
            out << "\n";
        }
    }
    MeshChecker checker (mesh, path);

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

    if (verbosity & Summary)
    {
        out << "  SUMMARY:\n" << checker.summary ();
    }
    if (verbosity & FileName)
    {
        out << (ret ? "  OK" : "  FAIL") << '\n';
    }

    if (!ret)
    {
        failCount++;
    }
    ok &= ret;
    out.flush ();
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

constexpr int NO_LEVELS = 4;
static int levels[NO_LEVELS] = {
    Mute,
    FileName,
    FileName | Summary,
    FileName | Summary | Details,
};

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
    parser.addOption (QCommandLineOption (QStringLiteral ("verbose"), QStringLiteral ("Show details of errors, 0 = mute, 1 = file name, 2 = + summary, 3 = + details"), QStringLiteral ("verbosity"), QStringLiteral ("1")));
    parser.addOption (QCommandLineOption (QStringList () << QStringLiteral ("multi-thread") << QStringLiteral ("mt"), QStringLiteral ("Multi threaded execution (experimental)")));
    parser.addOption (QCommandLineOption (QStringList () << QStringLiteral ("quiet") << QStringLiteral ("q"), QStringLiteral ("Only print names of incorrect files")));
    parser.addOption (QCommandLineOption (QStringList () << QStringLiteral ("CheckInfo"), QStringLiteral ("Show basic stats")));
    parser.addOption (QCommandLineOption (QStringList () << QStringLiteral ("CheckHoles"), QStringLiteral ("check for holes")));
    parser.addOption (QCommandLineOption (QStringList () << QStringLiteral ("CheckDuplicateTriangles"), QStringLiteral ("check for duplicate triangles")));
    parser.addOption (QCommandLineOption (QStringList () << QStringLiteral ("CheckShortEdges"), QStringLiteral ("check for short edges")));
    parser.addOption (QCommandLineOption (QStringList () << QStringLiteral ("CheckReversedTriangles"), QStringLiteral ("check for reversed triangles")));
    parser.addOption (QCommandLineOption (QStringList () << QStringLiteral ("CheckDuplicateVertices"), QStringLiteral ("check for duplicate vertices")));
    parser.addOption (QCommandLineOption (QStringList () << QStringLiteral ("CheckOpenEdges"), QStringLiteral ("check for open edges")));
    parser.addOption (QCommandLineOption (QStringList () << QStringLiteral ("CheckHalfEdgeOverlap"), QStringLiteral ("check for overlapping half edges")));
    parser.addOption (QCommandLineOption (QStringList () << QStringLiteral ("CheckTriangleOverlap"), QStringLiteral ("check for overlapping triangles")));
    parser.addOption (QCommandLineOption (QStringList () << QStringLiteral ("CheckUnviableTriangles"), QStringLiteral ("check for unviable triangles")));
    parser.addOption (QCommandLineOption (QStringList () << QStringLiteral ("CheckOverusedEdges"), QStringLiteral ("check for overused edges")));
    parser.addOption (QCommandLineOption (QStringList () << QStringLiteral ("CheckFlatTriangles"), QStringLiteral ("check for flat triangles")));
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
    if (parser.isSet (QByteArrayLiteral ("CheckInfo")))
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
    if (parser.isSet (QStringLiteral ("CheckInfo")))
    {
        flags |= MeshChecker::CheckInfo;
    }
    if (parser.isSet (QStringLiteral ("CheckUnviableTriangles")))
    {
        flags |= MeshChecker::CheckUnviableTriangles;
    }
    if (parser.isSet (QStringLiteral ("CheckOverusedEdges")))
    {
        flags |= MeshChecker::CheckOverusedEdges;
    }
    if (parser.isSet (QStringLiteral ("CheckFlatTriangles")))
    {
        flags |= MeshChecker::CheckFlatTriangles;
    }

    if (flags == 0)
    {
        flags = MeshChecker::Default;
    }

    if (parser.isSet ("mt"))
    {
        flags |= MeshChecker::MultiThread;
    }
    verbosity = (Verbosity)levels[std::min (parser.value (QStringLiteral ("verbose")).toInt (), NO_LEVELS - 1)];

    if (parser.positionalArguments ().isEmpty ())
    {
        qDebug ().nospace ().noquote () << "Missing argument";
        parser.showHelp (100);
    }

    QElapsedTimer et;
    et.start ();
    for (const auto& f : parser.positionalArguments ())
    {
        files (f);
    }
    if (verbosity != Mute && fileCount > 1)
    {
        out << QStringLiteral ("%1 failed out of %2 (%3% passed)").arg (failCount).arg (fileCount).arg (100 * (fileCount - failCount) / fileCount);
    }
    QLocale const locale;
    out << "\nTook " << locale.toString ((double)et.nsecsElapsed () / 1000000000.0) << " seconds" << '\n';
    return (ok ? 0 : 100);
}

#include <Mesh.h>

#include <QCommandLineParser>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>

#include "Document.h"
#include "MeshCheckerOutput.h"

static QStringList suffixes {"nethers", "stl", "obj", "3mf"};

static bool drawHoles = false;
static bool verbose = false;
static bool errors = false;
static bool quiet = false;

static bool  processFile (const QString& path)
{
    auto mesh = Document::readMesh (path);
    if (!mesh)
    {
        qDebug ().nospace ().noquote () << "Unable to open: \"" <<path << "\"\n";
    }
    if (!quiet)
    {
        qDebug().noquote().nospace() << path;
    }
    MeshCheckerOutput checker (mesh);
    checker.setVerboseReport (verbose);
    checker.setQuiet (quiet);
     if (drawHoles)
    {
        checker.setCheckFlag (MeshChecker::DrawHoles);
    }
    auto ret = checker.check ();
    errors &= ret;
    if (!ret && quiet)
    {
        qDebug().nospace().noquote() << "found errors in: " << path;
    }
    return ret;
}

static void files (const QString& path)
{
    QFileInfo const inf (path);
    if (!inf.exists())
    {
        qDebug().nospace().noquote() << path << " does not exist";
    }
    if (inf.isFile() && suffixes.contains(inf.suffix()))
    {
        processFile (path);
    }
    else if (inf.isDir())
    {
        QDir const d (path);
        auto entries = d.entryInfoList (QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
        for (const auto& e : entries)
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
    parser.addPositionalArgument (QStringLiteral ("mesh"), QStringLiteral ("3D file file to view (STL, 3MF or OBJ)"));
    parser.addOption (QCommandLineOption (QStringLiteral ("verbose"), QStringLiteral ("Show details of errors")));
    parser.addOption (QCommandLineOption (QStringList () << QStringLiteral ("drawholes") << QStringLiteral ("d"), QStringLiteral ("Generate images of holes")));
    parser.addOption (QCommandLineOption (QStringList () << QStringLiteral ("quiet") << QStringLiteral ("q"), QStringLiteral ("Only print names of incorrect files")));
    parser.process (a);

    if (parser.positionalArguments ().isEmpty ())
    {
        qDebug ().nospace ().noquote () << "Missing argument";
        parser.showHelp (100);
    }
    verbose = parser.isSet (QStringLiteral ("verbose"));
    drawHoles = parser.isSet (QStringLiteral ("drawholes"));
    quiet = parser.isSet (QStringLiteral ("quiet"));

    files (parser.positionalArguments ().at (0));

    return (errors ? 100 : 0);
}

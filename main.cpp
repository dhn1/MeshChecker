#include <Mesh.h>
#include <libSculptVersion.h>

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
static int flags = CheckNothing;
static int fileCount = 0;
static int failCount = 0;
static bool failOnly = false;
static bool reportedFilename = false;
static int summeryResult[64] = {};
static constexpr int NO_VERBOSITY_LEVELS = 4;

static int check2idx (Checks check)
{
    int idx = -1;
    auto t = (unsigned int)check;
    while (t)
    {
        t >>= 1;
        idx++;
    }
    return idx;
}

QTextStream out (stdout);

void report (const CheckResult& res)
{
    if (!reportedFilename)
    {
        reportedFilename = true;
        if (failOnly)
        {
        }
        else if (verbosity > FileName)
        {
            out << '\n';
        }
    }

    if (verbosity & Details)
    {
        out << res.m_report;
    }
    else if (verbosity & Summary)
    {
        out << res.m_report.split ('\n').constFirst () << '\n';
    }
    out.flush ();

    summeryResult[check2idx (res.m_check)] += res.m_badCount;
}

static bool processFile (const QString& path)
{
    reportedFilename = false;
    fileCount++;
    Triangle::resetID ();
    auto mesh = Document::readMesh (path);
    if (!mesh)
    {
        qDebug ().nospace ().noquote () << "Unable to open: \"" << path << "\"\n";
        failCount++;
        return false;
    }

    MeshChecker checker (mesh, path);

    checker.setCheckFlags (flags);

    if (!failOnly && (verbosity & FileName))
    {
        out << path;
    }

    bool ret;
    if (flags & MultiThread)
    {
        ret = checker.checkMultiThreaded ();
    }
    else
    {
        ret = checker.check ();
    }

    if (!failOnly || !ret)
    {
        if ((verbosity & Summary) && !checker.summary ().isEmpty ())
        {
            if (verbosity & FileName)
            {
                if (!(verbosity & (Summary | Details)))
                {
                    out << "\n";
                }
            }

            out << "  SUMMARY:\n" << checker.summary ();
        }

        if (!failOnly && (verbosity & FileName))
        {
            out << (ret ? "  OK" : "  FAIL") << '\n';
        }
        else if (failOnly && (verbosity & FileName) && !ret)
        {
            out << path << " FAIL" << '\n';
        }
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

static const int levels[NO_VERBOSITY_LEVELS] = {
    Mute,
    FileName,
    FileName | Summary,
    FileName | Summary | Details,
};

int main (int argc, char** argv)
{
    QGuiApplication const a (argc, argv);

    QCoreApplication::setApplicationName (QStringLiteral ("MeshChecker"));
    QCoreApplication::setApplicationVersion (QStringLiteral (VERSION) + QStringLiteral (" (libSculpt ") + libSculptVersion () + ")");
    QCoreApplication::setOrganizationName (QStringLiteral ("Netherwood Industries"));

    QCommandLineParser parser;
    parser.setApplicationDescription (QStringLiteral ("Check mesh for errors.\nNB, for 3mf files, only checks the main model (there can be more). Use 3mfsplitter app to work around this limit."));
    parser.addHelpOption ();
    parser.addVersionOption ();
    parser.addPositionalArgument (QStringLiteral ("mesh"), QStringLiteral ("3D file to examine (STL, 3MF or OBJ)"));
    parser.addOption (QCommandLineOption (QStringLiteral ("verbose"), QStringLiteral ("Show details of errors, 0 = mute, 1 = file name, 2 = + summary, 3 = + details"), QStringLiteral ("verbosity"), QStringLiteral ("1")));
    parser.addOption (QCommandLineOption (QStringList () << QStringLiteral ("multi-thread") << QStringLiteral ("mt"), QStringLiteral ("Multi threaded execution (experimental)")));
    parser.addOption (QCommandLineOption (QStringList () << QStringLiteral ("failOnly") << QStringLiteral ("f"), QStringLiteral ("Only print names of incorrect files")));
    parser.addOption (QCommandLineOption (QStringList () << QStringLiteral ("all") << QStringLiteral ("a"), QStringLiteral ("run all checks")));
    parser.addOption (QCommandLineOption (QStringList () << QStringLiteral ("critical") << QStringLiteral ("c"), QStringLiteral ("only do critical checks")));
    parser.addOption (QCommandLineOption (QStringList () << QStringLiteral ("stl"), QStringLiteral ("STL files only")));
    parser.addOption (QCommandLineOption (QStringList () << QStringLiteral ("3mf"), QStringLiteral ("3MF files only")));
    parser.addOption (QCommandLineOption (QStringList () << QStringLiteral ("nethers"), QStringLiteral ("nethers files only")));
    parser.setSingleDashWordOptionMode (QCommandLineParser::ParseAsLongOptions);
    const auto& checkList = MeshChecker::checkList ();
    for (const auto& check : checkList)
    {
        parser.addOption (QCommandLineOption (QStringList () << MeshChecker::optionName (check.first), MeshChecker::description (check.first)));
    }

    parser.process (a);

    if (parser.isSet ("stl") || parser.isSet (QStringLiteral ("3mf")) || parser.isSet (QStringLiteral ("nethers")))
    {
        suffixes.clear ();
    }
    if (parser.isSet (QStringLiteral ("stl")))
    {
        suffixes += QStringLiteral ("stl");
    }
    if (parser.isSet (QStringLiteral ("3mf")))
    {
        suffixes += QStringLiteral ("3mf");
    }
    if (parser.isSet (QStringLiteral ("nethers")))
    {
        suffixes += QStringLiteral ("nethers");
    }

    if (parser.isSet (QStringLiteral ("all")))
    {
        flags = All;
    }
    else if (parser.isSet (QStringLiteral ("critical")))
    {
        flags = Critical;
    }

    for (const auto& check : checkList)
    {
        if (parser.isSet (MeshChecker::optionName (check.first)))
        {
            flags |= check.first;
        }
    }

    if (flags == 0)
    {
        flags = Default;
    }

    if (parser.isSet (QStringLiteral ("mt")))
    {
        flags |= MultiThread;
    }
    failOnly = parser.isSet (QStringLiteral ("f"));
    verbosity = (Verbosity)levels[std::min (parser.value (QStringLiteral ("verbose")).toInt (), NO_VERBOSITY_LEVELS - 1)];

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
        out << QStringLiteral ("\n%1 failed out of %2 (%3% passed)").arg (failCount).arg (fileCount).arg (100 * (fileCount - failCount) / fileCount);
        out << "\n  Overall Summary:\n";
        int t = 1;
        int idx = 0;
        QLocale const locale;
        do
        {
            if (t & flags)
            {
                auto check = (Checks)(flags & t);
                auto r = summeryResult[check2idx (check)];
                if (r >= 0)
                {
                    const auto name = MeshChecker::checkName (check);
                    out << "    " << name << QString (padding - name.length (), QChar ('.')) << ": " << locale.toString (summeryResult[check2idx (check)]) << "\n";
                }
            }
            t <<= 1;
            idx++;
        } while (idx < 64);
    }

    if (verbosity != Mute)
    {
        QLocale const locale;
        out << "\nTook " << locale.toString ((double)et.nsecsElapsed () / 1000000000.0) << " seconds" << '\n';
    }

    return (ok ? 0 : 100);
}

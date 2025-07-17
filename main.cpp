#include <Mesh.h>
#include <libSculptVersion.h>

#include <QCommandLineParser>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QGuiApplication>
#include <QTextCursor>
#include <QTextDocument>

#include "Document.h"
#include "FileResult.h"
#include "MeshChecker.h"
#include "globals.h"

static Verbosity verbosity{Mute};
static bool genReport{};
static QStringList suffixes{"nethers", "stl", "obj", "3mf"};

static bool ok = true;
static int flags = CheckNothing;
static int fileCount = 0;
static int failCount = 0;
static bool failOnly = false;
static int summeryResult[64] = {};
static constexpr int NO_VERBOSITY_LEVELS = 4;
static QString rootFolder;
static bool verboseFail = false;

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
static QFile markdown;
static QTextStream md (&markdown);

static void reportMd (const FileResult& results)
{
    if (results.m_pass && failOnly)
    {
        return;
    }
    auto path = results.m_path;
    if (path.startsWith (rootFolder))
    {
        path = path.mid (rootFolder.length ());
        if (path.startsWith (QStringLiteral ("/")))
        {
            path = QStringLiteral ("./") + path.mid (1);
        }
    }
    md << "## " << path << (results.m_pass ? " - PASS\n" : " - FAIL\n");
    if (results.m_pass && verboseFail)
    {
        return;
    }
    if (verbosity & Summary)
    {
        for (const auto& res : results.m_checkResults)
        {
            if (res.m_check == CheckInfo /*|| res.m_check == CheckShortEdges*/)
            {
                auto report = res.m_report.trimmed ();
                report = report.mid (4).trimmed ();
                md << "    " << report << "\n";
            }
        }
    }

    for (const auto& res : results.m_checkResults)
    {
        summeryResult[check2idx (res.m_check)] += res.m_badCount;
    }

    if (verbosity & Summary)
    {
        md << "|Test|Result|\n|--|--|\n";

        for (const auto& res : results.m_checkResults)
        {
            if (res.m_check == CheckInfo || res.m_check == CheckShortEdges)
            {
                continue;
            }
            switch (res.m_badCount)
            {
            case -1:
                md << "|" << res.m_report.split ('\n').constFirst () << "||\n";
                break;
            case 0:
                md << "|" << MeshChecker::checkName (res.m_check) << "|None|\n";
                break;
            default:
                md << "|" << MeshChecker::checkName (res.m_check) << "|" << QString::number (res.m_badCount) << "|\n";
                break;
            }
        }
        md << '\n';
    }
}

static void reportBasic (const FileResult& results)
{
    if (results.m_pass && failOnly)
    {
        return;
    }
    if (verbosity > FileName)
    {
        out << results.m_path << (results.m_pass ? " - PASS\n" : " - FAIL\n");
    }

    for (const auto& res : results.m_checkResults)
    {
        if (verbosity & Details)
        {
            out << res.m_report;
        }
        else if (verbosity & Summary)
        {
            out << res.m_report.split ('\n').constFirst () << '\n';
        }
        summeryResult[check2idx (res.m_check)] += res.m_badCount;
    }

    if (verbosity & Summary)
    {
        out << "  SUMMARY:\n";
    }
    QLocale const locale;
    for (const auto& res : results.m_checkResults)
    {
        if (verbosity & Summary)
        {
            if (res.m_badCount > -1)
            {
                auto name = MeshChecker::checkName (res.m_check);
                out << "    " << name + QString (padding - name.length (), QChar ('.')) << QStringLiteral (": ") + locale.toString (res.m_badCount) + QStringLiteral ("\n");
            }
        }
    }
    out.flush ();
}

static bool processFile (const QString& path)
{
    fileCount++;
    Triangle::resetID ();  // May need to mutex this if we multi thread
    auto mesh = Document::readMesh (path);
    if (!mesh)
    {
        qDebug ().nospace ().noquote () << "Unable to open: \"" << path << "\"\n";
        failCount++;
        return false;
    }

    MeshChecker checker (mesh, path);

    checker.setCheckFlags (flags);

    auto res = checker.check ();

    if (genReport)
    {
        reportMd (res);
    }
    else
    {
        reportBasic (res);
    }

    if (!res.m_pass)
    {
        failCount++;
    }
    ok &= res.m_pass;
    out.flush ();
    return res.m_pass;
}

static void files (const QString& path)
{
    QFileInfo const inf (path);
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

static void rootFiles (const QString& path)
{
    QFileInfo const inf (path);
    if (!inf.exists ())
    {
        qDebug ().nospace ().noquote () << path << " does not exist";
    }
    if (inf.isDir())
    {
        rootFolder = inf.absoluteFilePath ();
    }
    else
    {
        rootFolder = inf.absolutePath ();
    }
    files (path);
}

static void summarise ()
{
    out << QStringLiteral ("\n  Overall Summary: %1 failed out of %2 (%3% passed)\n").arg (failCount).arg (fileCount).arg (100 * (fileCount - failCount) / fileCount);
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

static void summariseMd ()
{
    md << "# Overall Summary:\n";
    md << QStringLiteral ("\n%1 failed out of %2 (%3% passed)").arg (failCount).arg (fileCount).arg (100 * (fileCount - failCount) / fileCount) << "\n";
    if (!(verbosity & Summary))
    {
        return;
    }
    md << "|||\n|---|---|\n";

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
                const auto badCount = summeryResult[check2idx (check)];
                if (badCount == 0)
                {
                    md << "|" << name << "|None|\n";
                }
                else
                {
                    md << "|" << name << "|" << locale.toString (summeryResult[check2idx (check)]) << "|\n";
                }
            }
        }
        t <<= 1;
        idx++;
    } while (idx < 64);
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
    QCoreApplication::setApplicationVersion (QStringLiteral (VERSION) + QStringLiteral (" (libSculpt ") + libSculptVersion () + QStringLiteral (")"));
    QCoreApplication::setOrganizationName (QStringLiteral ("Netherwood Industries"));

    QCommandLineParser parser;
    parser.setApplicationDescription (QStringLiteral ("Check mesh for errors.\nNB, for 3mf files, only checks the main model (there can be more). Use 3mfsplitter app to work around this limit."));
    parser.addHelpOption ();
    parser.addVersionOption ();
    parser.addPositionalArgument (QStringLiteral ("mesh"), QStringLiteral ("3D file to examine (STL, 3MF or OBJ)"));
    parser.addOption (QCommandLineOption (QStringLiteral ("verbose"), QStringLiteral ("Show details of errors, 0 = mute, 1 = file name, 2 = + summary, 3 = + details"), QStringLiteral ("verbosity"), QStringLiteral ("1")));
    //parser.addOption (QCommandLineOption (QStringList () << QStringLiteral ("multi-thread") << QStringLiteral ("mt"), QStringLiteral ("Multi threaded execution (experimental)")));
    parser.addOption (QCommandLineOption (QStringList () << QStringLiteral ("failOnly") << QStringLiteral ("f"), QStringLiteral ("Only print names of incorrect files")));
    parser.addOption (QCommandLineOption (QStringList () << QStringLiteral ("all") << QStringLiteral ("a"), QStringLiteral ("run all checks")));
    parser.addOption (QCommandLineOption (QStringList () << QStringLiteral ("critical") << QStringLiteral ("c"), QStringLiteral ("only do critical checks")));
    parser.addOption (QCommandLineOption (QStringList () << QStringLiteral ("stl"), QStringLiteral ("STL files only")));
    parser.addOption (QCommandLineOption (QStringList () << QStringLiteral ("3mf"), QStringLiteral ("3MF files only")));
    parser.addOption (QCommandLineOption (QStringList () << QStringLiteral ("nethers"), QStringLiteral ("nethers files only")));
    parser.addOption (QCommandLineOption (QStringList () << QStringLiteral ("report"), QStringLiteral ("generate a report in Mark Down format"), QStringLiteral ("file")));
    parser.addOption (QCommandLineOption (QStringList () << QStringLiteral ("verboseFail"), QStringLiteral ("generate a summery or detailed report only for failed mesh files")));
    parser.setSingleDashWordOptionMode (QCommandLineParser::ParseAsLongOptions);
    const auto& checkList = MeshChecker::checkList ();
    for (const auto& check : checkList)
    {
        parser.addOption (QCommandLineOption (QStringList () << MeshChecker::optionName (check.first), MeshChecker::description (check.first)));
    }

    parser.process (a);

    verboseFail = parser.isSet (QStringLiteral ("verboseFail"));
    if (parser.isSet (QStringLiteral ("stl")) || parser.isSet (QStringLiteral ("3mf")) || parser.isSet (QStringLiteral ("nethers")))
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

    // if (parser.isSet (QStringLiteral ("mt")))
    // {
    //     flags |= MultiThread;
    // }
    failOnly = parser.isSet (QStringLiteral ("f"));
    verbosity = (Verbosity)levels[std::min (parser.value (QStringLiteral ("verbose")).toInt (), NO_VERBOSITY_LEVELS - 1)];

    genReport = parser.isSet (QStringLiteral ("report"));

    if (genReport)
    {
        markdown.setFileName (parser.value (QStringLiteral ("report")));
        markdown.open (QFile::WriteOnly);
        if (!markdown.isOpen ())
        {
            qDebug () << "Unable to open report output file: " << parser.value (QStringLiteral ("report"));
            return 100;
        }
        md << "# Mesh Check report\n";
        md << "Version: " << +VERSION << "\n\n";
        if (failOnly)
        {
            md << "Reporting only failed files.\n\n";
        }
        md << "Running checks:\n";
        int t = 1;
        do
        {
            if (t & flags)
            {
                auto check = (Checks)(flags & t);
                md << "* " << MeshChecker::checkName (check) << " - " << MeshChecker::description (check) << "\n";
            }
            t <<= 1;
        } while (t);
        md << "\n";
    }

    if (parser.positionalArguments ().isEmpty ())
    {
        qDebug ().nospace ().noquote () << "Missing argument";
        parser.showHelp (100);
    }

    QElapsedTimer et;
    et.start ();
    for (const auto& f : parser.positionalArguments ())
    {
        rootFiles (f);
    }

    if (verbosity != Mute && fileCount > 1)
    {
        if (genReport)
        {
            summariseMd ();
        }
        else
        {
            summarise ();
        }
    }

    if (verbosity != Mute)
    {
        QLocale const locale;
        out << "\n" << a.applicationName() << " took " << locale.toString ((double)et.nsecsElapsed () / 1000000000.0) << " seconds" << '\n';
    }

    return (ok ? 0 : 100);
}

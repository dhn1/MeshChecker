#include <ColourFactory.h>
#include <Mesh.h>
#include <libSculptVersion.h>
#include <libSculptVersionStatic.h>

#include <QCommandLineParser>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QTextCursor>
#include <QTextDocument>
#include <QtConcurrent>

#include "Document.h"
#include "FileResult.h"
#include "MeshChecker.h"
#include "globals.h"

static Verbosity verbosity{Mute};
static bool genReport{};
static QStringList suffixes{"nethers", "stl", "obj", "3mf"};

static bool ok = true;
static uint flags = CheckNothing;
static int fileCount = 0;
static int failCount = 0;
static bool failOnly = false;
static enum : uint8_t { None, Record, Compare } recordMode{None};
static int summaryResult[64] = {};
static int diffSum[64] = {};
static constexpr int NO_VERBOSITY_LEVELS = 4;
static QString rootFolder;
static bool verboseFail = false;
QTextStream out (stdout);
static QFile markdown;
static QTextStream md (&markdown);
static QJsonObject recording;
static QJsonObject recordingFiles;
static QString folderFilter;
static QStringList fileList;
static const int levels[NO_VERBOSITY_LEVELS] = {
    Mute,
    FileName,
    FileName | Summary,
    FileName | Summary | Details,
};
static bool markFiles{};
static bool markIslands{};
static QFutureSynchronizer<void> synchroniser;
static QMutex updateMutex;

static void generateFileList ()
{
    for (auto it = recordingFiles.begin (); it != recordingFiles.end (); it++)
    {
        fileList.push_back (it.key ());
    }
}

static QString diffNo (int no)
{
    static const QLocale locale;
    if (no < 0)
    {
        return locale.toString (no);
    }
    return QStringLiteral ("+") + locale.toString (no);
}

static int check2idx (Checks check)
{
    int idx = -1;
    auto t = (unsigned int)check;
    while (t)
    {
        t >>= 1U;
        idx++;
    }
    return idx;
}

static void record (const FileResult& fileResult)
{
    switch (recordMode)
    {
    case None:
        break;

    case Record:
        {
            QJsonObject file;
            QJsonObject o;
            for (const auto& result : fileResult.checkResults ())
            {
                o.insert (MeshChecker::checkName (result.check ()), result.badCount ());
            }
            file.insert (QStringLiteral ("checks"), o);
            file.insert (QStringLiteral ("pass"), fileResult.pass ());
            recordingFiles.insert (fileResult.path (), file);
        }
        break;

    case Compare:
        {
            const QFileInfo fi (fileResult.path ());
            if (fi.lastModified ().secsTo (QDateTime::currentDateTime ()) > 40LL * 60LL)
            {
                qDebug ().noquote ().nospace () << "Out of date file? \"" << fileResult.path () << "\"";
            }
            bool changes = false;
            auto file = recordingFiles.value (fileResult.path ()).toObject ();
            if (file.isEmpty ())
            {
                out << "\n" << fileResult.path () << " - NEW FILE - " << (fileResult.pass () ? "PASS" : "FAIL") << '\n';
                //return;
            }
            auto o = file.value (QStringLiteral ("checks")).toObject ();

            for (const auto& result : fileResult.checkResults ())
            {
                if (result.badCount () == -1)
                {
                    continue;
                }
                auto old = o.value (result.name ()).toInt ();
                auto diff = result.badCount () - old;

                diffSum[check2idx (result.check ())] += diff;
                if (result.badCount () != old)
                {
                    changes = true;
                }
            }
            changes |= (fileResult.pass () != file.value (QStringLiteral ("pass")).toBool ());
            if (changes)
            {
                out << '\n' << fileResult.path ();
                if (fileResult.pass () != file.value (QStringLiteral ("pass")).toBool ())
                {
                    out << " " << (file.value (QStringLiteral ("pass")).toBool () ? "PASS" : "FAIL") << " -> " << (fileResult.pass () ? "PASS" : "FAIL");
                }
                else
                {
                    out << " " << (file.value (QStringLiteral ("pass")).toBool () ? "PASS" : "FAIL");
                }
                out << "\n";

                for (const auto& result : fileResult.checkResults ())
                {
                    if (result.badCount () == -1)
                    {
                        continue;
                    }
                    auto name = result.name ();
                    auto old = o.value (name).toInt ();

                    auto diff = result.badCount () - old;
                    if (diff != 0)
                    {
                        out << "  " << name << QString (padding - name.length (), QChar ('.')) << ": " << old << " -> " << result.badCount () << " " << diffNo (diff) << '\n';
                    }
                }
            }
        }
        break;
    }
}

static void reportMd (const FileResult& result)
{
    if (result.pass () && failOnly)
    {
        return;
    }
    auto path = result.path ();
    if (path.startsWith (rootFolder))
    {
        path = path.mid (rootFolder.length ());
        if (path.startsWith (QStringLiteral ("/")))
        {
            path = QStringLiteral ("./") + path.mid (1);
        }
    }
    md << "## " << path << (result.pass () ? " - PASS\n" : " - FAIL\n");
    if (result.pass () && verboseFail)
    {
        return;
    }

    if (verbosity & Details)
    {
        for (const auto& res : result.checkResults ())
        {
            if (res.check () == CheckInfo)  // Info first
            {
                auto report = res.report ().trimmed ();
                report = report.mid (4).trimmed ();
                md << "    " << report << "\n";
            }
        }
    }

    if (verbosity & Summary || (!result.pass () && verboseFail))
    {
        md << "|Check|Result|\n|---|---|\n";

        for (const auto& res : result.checkResults ())
        {
            if (res.check () == CheckInfo)
            {
                continue;
            }
            switch (res.badCount ())
            {
            case -1:
                md << "|" << res.report ().split ('\n').constFirst () << "||\n";
                break;
            case 0:
                md << "|" << (!res.pass () ? "**" : "") << res.name () << (!res.pass () ? "**" : "") << "|" << (!res.pass () ? "**None|\n" : "None|\n");
                break;
            default:
                md << "|" << (!res.pass () ? "**" : "") << res.name () << (!res.pass () ? "**" : "") << "|" << (!res.pass () ? "**" : "") << QString::number (res.badCount ()) << (!res.pass () ? "**" : "") << "|\n";
                break;
            }
        }
        md << '\n';
    }
}

static void reportBasic (const FileResult& result)
{
    if (result.pass () && failOnly)
    {
        return;
    }
    if (verbosity > FileName)
    {
        out << result.path () << (result.pass () ? " - PASS\n" : " - FAIL\n");
    }

    for (const auto& res : result.checkResults ())
    {
        if (verbosity & Details)
        {
            out << res.report ();
        }
        else if (verbosity & Summary)
        {
            out << res.report ().split ('\n').constFirst () << '\n';
        }
    }

    if (verbosity & Summary)
    {
        out << "\n  SUMMARY:\n";
    }
    QLocale const locale;
    for (const auto& res : result.checkResults ())
    {
        if (verbosity & Summary)
        {
            if (res.badCount () > -1)
            {
                auto name = res.name ();
                out << "    " << name + QString (padding - name.length (), QChar ('.')) << QStringLiteral (": ") + locale.toString (res.badCount ()) + (res.pass () ? " pass\n" : " FAIL\n");
            }
        }
    }
    out.flush ();
}
static void callback (Checks check, const MeshPtr& mesh, const TrianglePtr& t1, const TrianglePtr& t2)
{
    Q_UNUSED (check)
    Q_UNUSED (mesh)

    static auto cf = ColourFactory::instance ();
    static auto col = cf->colour (1.0, 0.0, 1.0);
    static auto colours = cf->stockColours ();
    static int idx;

    Q_ASSERT (t1->id () | t2->id ());
    t1->setColour (colours.at (idx));
    idx = (idx + 1) % colours.count ();
    t2->setColour (colours.at (idx));
    idx = (idx + 1) % colours.count ();
}

static void processFile (const QString& path)
{
    fileList.removeAll (path);
    fileCount++;
    Triangle::resetID ();  // May need to mutex this if we multi thread
    auto mesh = Document::readMesh (path);
    if (!mesh)
    {
        qDebug ().nospace ().noquote () << "Unable to open: \"" << path << "\"\n";
        failCount++;
        return;
    }

    auto checker = new MeshChecker;
    auto future = QtConcurrent::run ([mesh, path, checker] {
        checker->setData (mesh, path);

        if (markFiles)
        {
            mesh->removeColour ();
            checker->setCallback (callback);
        }

        checker->setCheckFlags (flags);

        const auto res = checker->check ();

        QMutexLocker locker (&updateMutex);
        for (const auto& c : res.checkResults ())
        {
            if (c.badCount () > 0)
            {
                summaryResult[check2idx (c.check ())] += c.badCount ();
            }
        }

        if (genReport)
        {
            reportMd (res);
        }
        else
        {
            reportBasic (res);
        }

        record (res);

        if (!res.pass ())
        {
            failCount++;
        }
        ok &= res.pass ();
        out.flush ();

        if (markIslands)
        {
            mesh->removeColour ();
            auto cols = ColourFactory::instance ()->stockColours ();
            int colIdx = 0;
            auto hes = HalfEdges::create (mesh);
            auto holes = hes->holes ();
            for (const auto& hole : std::as_const (holes))
            {
                if (hole->testFlag (Hole::Island))
                {
                    hole->colourHole (cols.at (colIdx++));
                    colIdx %= cols.size ();
                }
            }
        }
        if (markFiles || markIslands)
        {
            const QFileInfo fi (path);
            auto fname = fi.canonicalPath () + QStringLiteral ("/") + fi.baseName () + QStringLiteral (".nethers");
            //qDebug () << path;
            Document::write (mesh, path);
        }
        delete checker;
    });
    synchroniser.addFuture (future);
}

static void files (const QString& path)
{
    QFileInfo const inf (path);

    if (inf.isFile () && suffixes.contains (inf.suffix ()) && inf.path ().endsWith (folderFilter))
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
    if (inf.isDir ())
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
    uint t = 1;
    int idx = 0;
    QLocale const locale;
    do
    {
        if (t & flags)
        {
            auto check = (Checks)(flags & t);
            auto r = summaryResult[check2idx (check)];
            if (r >= 0)
            {
                const auto name = MeshChecker::checkName (check);
                out << "    " << name << QString (padding - name.length (), QChar ('.')) << ": " << locale.toString (summaryResult[check2idx (check)]) << "\n";
            }
        }
        t <<= 1U;
        idx++;
    } while (idx < 64);
}

static void summariseMd ()
{
    md << "# Overall Summary:\n";
    md << QStringLiteral ("\n%1 failed out of %2 (%3% passed)").arg (failCount).arg (fileCount).arg (100 * (fileCount - failCount) / fileCount) << "\n";
    if (!(verbosity & Summary) && !(failCount > 0 && verboseFail))
    {
        return;
    }
    md << "\n|Check|Result|\n";
    md << "|---|---|\n";

    uint t = 1;
    int idx = 0;
    QLocale const locale;
    do
    {
        if (t & flags)
        {
            auto check = (Checks)(flags & t);
            auto r = summaryResult[check2idx (check)];
            if (r >= 0 || (failCount > 0 && verboseFail))
            {
                const auto name = MeshChecker::checkName (check);
                const auto badCount = summaryResult[check2idx (check)];
                if (badCount == 0)
                {
                    md << "|" << name << "|None|\n";
                }
                else
                {
                    md << "|" << name << "|" << locale.toString (summaryResult[check2idx (check)]) << "|\n";
                }
            }
        }
        t <<= 1U;
        idx++;
    } while (idx < 64);
}

int main (int argc, char** argv)
{
    QGuiApplication const a (argc, argv);

#ifdef QT_DEBUG
    const QDir dir;
    dir.mkpath (QStringLiteral ("/tmp/3d"));
#endif

    QCoreApplication::setApplicationName (QStringLiteral ("MeshChecker"));
    QCoreApplication::setApplicationVersion (QStringLiteral (VERSION) + QStringLiteral (" (libSculpt build %1, in use %2)").arg (LIB_SCULPT_VERSION).arg (libSculptVersion ()));
    QCoreApplication::setOrganizationName (QStringLiteral ("Netherwood Industries"));

    QCommandLineParser parser;
    parser.setApplicationDescription (QStringLiteral ("Check mesh for errors.\nNB, for 3mf files, only checks the main model (there can be more). Use 3mfsplitter app to work around this limitation."));
    parser.addHelpOption ();
    parser.addVersionOption ();
    parser.addPositionalArgument (QStringLiteral ("<mesh>"), QStringLiteral ("3D file to examine (STL, 3MF or OBJ)."));
    parser.addOption (QCommandLineOption (QStringLiteral ("verbose"), QStringLiteral ("Show details of errors, 0 = mute, 1 = file name, 2 = + summary, 3 = + details."), QStringLiteral ("verbosity"), QStringLiteral ("1")));
    //parser.addOption (QCommandLineOption (QStringList () << QStringLiteral ("multi-thread") << QStringLiteral ("mt"), QStringLiteral ("Multi threaded execution (experimental).")));
    parser.addOption (QCommandLineOption (QStringList () << QStringLiteral ("fail-only") << QStringLiteral ("f"), QStringLiteral ("Only print names of incorrect files.")));
    parser.addOption (QCommandLineOption (QStringList () << QStringLiteral ("all") << QStringLiteral ("a"), QStringLiteral ("Run all checks.")));
    parser.addOption (QCommandLineOption (QStringList () << QStringLiteral ("default") << QStringLiteral ("d"), QStringLiteral ("Run default checks.")));
    parser.addOption (QCommandLineOption (QStringList () << QStringLiteral ("cmpDefault"), QStringLiteral ("Run default checks for comparisons.")));
    parser.addOption (QCommandLineOption (QStringList () << QStringLiteral ("critical") << QStringLiteral ("c"), QStringLiteral ("Only do critical checks.")));
    parser.addOption (QCommandLineOption (QStringList () << QStringLiteral ("stl"), QStringLiteral ("STL files only.")));
    parser.addOption (QCommandLineOption (QStringList () << QStringLiteral ("3mf"), QStringLiteral ("3MF files only.")));
    parser.addOption (QCommandLineOption (QStringList () << QStringLiteral ("nethers"), QStringLiteral ("nethers files only.")));
    parser.addOption (QCommandLineOption (QStringList () << QStringLiteral ("report"), QStringLiteral ("Generate a report in Mark Down format."), QStringLiteral ("file")));
    parser.addOption (QCommandLineOption (QStringList () << QStringLiteral ("verbose-fail"), QStringLiteral ("Generate a summery or detailed report only for failed mesh files.")));
    parser.addOption (QCommandLineOption (QStringList () << QStringLiteral ("compare"), QStringLiteral ("Compare with last recorded run."), QStringLiteral ("JSON file")));
    parser.addOption (QCommandLineOption (QStringList () << QStringLiteral ("record"), QStringLiteral ("Record results in file for use with compare."), QStringLiteral ("JSON file")));
    parser.addOption (QCommandLineOption (QStringList () << QStringLiteral ("folderFilter") << QStringLiteral ("ff"), QStringLiteral ("Only look in subfolders with given name."), QStringLiteral ("folder name")));
    parser.addOption (QCommandLineOption (QStringList () << QStringLiteral ("mark"), QStringLiteral ("Mark bad triangles with colour and save file as .nether type.")));
    parser.addOption (QCommandLineOption (QStringList () << QStringLiteral ("mark-islands"), QStringLiteral ("Mark islands with colours and save file as .nether type.")));
    parser.addOption (QCommandLineOption (QStringList () << QStringLiteral ("hyperlinks"), QStringLiteral ("Produce markdown text with hyper-links")));
    parser.addOption (QCommandLineOption (QStringList () << QStringLiteral ("multi-use-edges"), QStringLiteral ("Allow edges to be used in more than just a pair")));
    parser.addOption (QCommandLineOption (QStringList () << QStringLiteral ("list-limit"), QStringLiteral ("Allow edges to be used in more than just a pair"), QStringLiteral ("limit")));

    //parser.setSingleDashWordOptionMode (QCommandLineParser::ParseAsLongOptions);
    const auto& checkList = MeshChecker::checkList ();
    for (const auto& check : checkList)
    {
        parser.addOption (QCommandLineOption (QStringList () << MeshChecker::optionName (check.first), MeshChecker::description (check.first) + '.'));
    }

    parser.process (a);

    markFiles = parser.isSet (QStringLiteral ("mark"));
    markIslands = parser.isSet (QStringLiteral ("mark-islands"));
    MeshChecker::setViewerHyperlinks (parser.isSet (QStringLiteral ("hyperlinks")));
    MeshChecker::setAllowOverusedEdges (parser.isSet (QStringLiteral ("multi-use-edges")));

    if (parser.isSet ("list-limit"))
    {
        MeshChecker::setListLimit (parser.value ("list-limit").toInt ());
    }
    if (parser.isSet (QStringLiteral ("compare")))
    {
        if (parser.isSet (QStringLiteral ("record")))
        {
            qDebug ().nospace ().noquote () << "Only --record or --compare allowed, not both";
            return 106;
        }
        QFile in (parser.value (QStringLiteral ("compare")));

        if (!in.open (QFile::ReadOnly))
        {
            qDebug ().nospace ().noquote () << "Unable to open \"" << parser.value (QStringLiteral ("compare")) << "\"";
            return 105;
        }
        recordMode = Compare;
        recording = QJsonDocument::fromJson (in.readAll ()).object ();
        if (recording.isEmpty ())
        {
            qDebug ().nospace ().noquote () << "Unable t open: \"" << in.fileName () << "\"";
            return 100;
        }
        recordingFiles = recording.value (QStringLiteral ("files")).toObject ();

        generateFileList ();
    }

    if (parser.isSet (QStringLiteral ("record")))
    {
        if (parser.isSet (QStringLiteral ("compare")))
        {
            qDebug ().nospace ().noquote () << "Only --record or --compare allowed, not both";
            return 106;
        }
        recordMode = Record;
        recording.insert (QStringLiteral ("version"), qApp->applicationVersion ());
    }

    folderFilter = parser.value (QStringLiteral ("ff"));

    verboseFail = parser.isSet (QStringLiteral ("verbose-fail"));
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
    if (parser.isSet (QStringLiteral ("default")))
    {
        flags = Default;
    }
    if (parser.isSet (QStringLiteral ("cmpDefault")))
    {
        flags = CmpDefault;
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
        if (!markdown.open (QFile::WriteOnly))
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
        uint t = 1;
        do
        {
            if (t & flags)
            {
                auto check = (Checks)(flags & t);
                md << "* " << MeshChecker::checkName (check) << " - " << MeshChecker::description (check) << "\n";
            }
            t <<= 1U;
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

    synchroniser.waitForFinished ();

    if (genReport)
    {
        summariseMd ();
    }

    if ((verbosity & Summary) && fileCount > 1)
    {
        summarise ();
    }

    if (recordMode == Record)
    {
        recording.insert (QStringLiteral ("fileCount"), fileCount);
        recording.insert (QStringLiteral ("fails"), failCount);
        recording.insert (QStringLiteral ("files"), recordingFiles);

        QFile out (parser.value (QStringLiteral ("record")));
        if (!out.open (QFile::WriteOnly))
        {
            qDebug ().nospace ().noquote () << "Unable to open \"" << parser.value (QStringLiteral ("record")) << "\"";
            return 105;
        }
        const QJsonDocument doc (recording);
        out.write (doc.toJson ());
    }
    if (recordMode == Compare)
    {
        for (const auto& file : std::as_const (fileList))
        {
            out << "Missing file: \"" << file << "\"\n";
        }
        out << QStringLiteral ("\nOverall changes: files: %1 (%2), fails: %3 (%4)\n")
                   .arg (fileCount)
                   .arg (diffNo (fileCount - recording.value (QStringLiteral ("fileCount")).toInt ()))
                   .arg (failCount)
                   .arg (diffNo (failCount - recording.value (QStringLiteral ("fails")).toInt ()));
        uint t = 1;
        int idx = 0;
        do
        {
            if (t & flags)
            {
                auto check = (Checks)(flags & t);
                const auto name = MeshChecker::checkName (check);
                int const idx = check2idx (check);
                auto r = diffSum[check2idx (check)];
                if (r != 0)
                {
                    out << "  " << name << QString (padding - name.length (), QChar ('.')) << ": " << summaryResult[idx] - diffSum[idx] << " -> " << summaryResult[idx] << " (" << diffNo (diffSum[idx]) << ")\n";
                }
                else
                {
                    out << "  " << name << QString (padding - name.length (), QChar ('.')) << ": " << summaryResult[idx] << "\n";
                }
            }
            t <<= 1U;
            idx++;
        } while (idx < 64);
    }

    if (verbosity != Mute)
    {
        QLocale const locale;
        out << "\n" << QGuiApplication::applicationName () << " took " << locale.toString ((double)et.nsecsElapsed () / 1000000000.0) << " seconds" << '\n';
    }

    return (ok ? 0 : 100);
}

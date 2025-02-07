#include <DocumentNethers.h>
#include <Mesh.h>

#include <QCommandLineParser>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QGuiApplication>

#include "Document.h"
#include "MeshCheckerOutput.h"
#include "globals.h"

static bool verbose = false;
static bool errors = false;
static bool quiet = false;
QTextStream out (stdout);
static QString outputPath ("/tmp/3d");

static bool splitHoles (const QString& path)
{
    auto mesh = Document::readMesh (path);
    if (!mesh)
    {
        qDebug ().nospace ().noquote () << "Unable to open: \"" << path << "\"\n";
        return false;
    }
    if (!quiet)
    {
        qDebug ().noquote ().nospace () << path;
    }
    auto edges = HalfEdges::create (mesh);
    auto edgeMap = edges->generateEdgeHashByVertex1 ();

    auto nextHalfEdge = [&edgeMap] (const HalfEdgePtr lastEdge) -> HalfEdgePtr {
        auto range = edgeMap.equal_range (lastEdge->v2 ());
        for (auto it = range.first; it != range.second; it++)
        {
            auto e = *it;
            if (!e->m_pair && !e->testFlag (HalfEdge::Processed))
            {
                return e;
            }
        }
        return {};
    };

    int idx = 0;
    SegmentList sl;
    for (auto& he : edges->halfEdgeList ())
    {
        TriangleList ts;
        QList<VertexPtr> vs;
        sl.clear ();

        if (!he->m_pair && !he->testFlag (HalfEdge::Processed))
        {
            // An openEdge!j
            while (he)
            {
                ts.push_back (he->m_triangle);
                vs.push_back (he->v1 ());
                sl.push_back (Segment::create (he->v1 (), he->v2 ()));

                he->setFlag (HalfEdge::Processed);
                he = nextHalfEdge (he);
            }

            auto m = Mesh::create (ts);
            DocumentNethers doc (m);
            doc.add (sl);
            doc.write (outputPath + QStringLiteral ("/hole_%1.nethers").arg (idx++));

            for (int i = 0; i < vs.count (); i++)
            {
                for (int j = i + 1; j < vs.count (); j++)
                {
                    qDebug () << vs.at (i)->magnitude (vs.at (j)) << vs.at (i)->manhatten (vs.at (j));
                }
            }
            qDebug () << "***";
        }
    }
    if (idx > 0)
    {
        qDebug ().noquote ().nospace () << idx << " holes";
    }
    return idx == 0;
}

int main (int argc, char** argv)
{
    QGuiApplication const a (argc, argv);

    QCoreApplication::setApplicationName (QStringLiteral ("HoleSplitter"));
    QCoreApplication::setApplicationVersion (QStringLiteral (VERSION));
    QCoreApplication::setOrganizationName (QStringLiteral ("Netherwood Industries"));

    QCommandLineParser parser;
    parser.setApplicationDescription (QStringLiteral ("Pick out holes in triangular meshes"));
    parser.addHelpOption ();
    parser.addVersionOption ();
    parser.addPositionalArgument (QStringLiteral ("mesh"), QStringLiteral ("3D file to examine (STL, 3MF or OBJ)"));
    parser.addOption (QCommandLineOption (QStringLiteral ("verbose"), QStringLiteral ("Show details of errors")));

    parser.process (a);

    if (parser.positionalArguments ().isEmpty ())
    {
        qDebug ().nospace ().noquote () << "Missing argument";
        parser.showHelp (100);
    }
    verbose = parser.isSet (QStringLiteral ("verbose"));

    QElapsedTimer et;
    et.start ();
    splitHoles (parser.positionalArguments ().at (0));
    QLocale const locale;
    out << "Took " << locale.toString ((double)et.nsecsElapsed () / 1000000000.0) << "seconds" << '\n';
    return (errors ? 100 : 0);
}

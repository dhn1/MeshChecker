
#include "MeshCheckerOutput.h"

#include "globals.h"

void MeshCheckerOutput::setVerbose (bool newVerbose)
{
    m_verboseReport = newVerbose;
}

bool MeshCheckerOutput::quiet () const
{
    return m_quiet;
}

void MeshCheckerOutput::setQuiet (bool newQuiet)
{
    m_quiet = newQuiet;
}

void MeshCheckerOutput::report (const QString& str)
{
    if (!m_quiet)
    {
        out << "  " << str << '\n';
        out.flush ();
    }
}

void MeshCheckerOutput::verbose (const QString& str)
{
    if (m_verboseReport)
    {
        if (str.startsWith (QStringLiteral ("  ")))
        {
            out << str << '\n';
        }
        else
        {
            out << "  " << str << '\n';
        }
        out.flush ();
    }
}

bool MeshCheckerOutput::verboseReport () const
{
    return m_verboseReport;
}

void MeshCheckerOutput::setVerboseReport (bool newVerboseReport)
{
    m_verboseReport = newVerboseReport;
}

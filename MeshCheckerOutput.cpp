
#include "MeshCheckerOutput.h"

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

void MeshCheckerOutput::report (const QString &str)
{
    if (!m_quiet)
    {
        qDebug ().nospace ().noquote () << "   " << str;
    }
}

void MeshCheckerOutput::verbose (const QString &str)
{
    if (m_verboseReport)
    {
        if (str.startsWith("  "))
        {
            qDebug ().nospace ().noquote () << str;
        }
        else
        {
            qDebug ().nospace ().noquote () << "   " << str;
        }
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

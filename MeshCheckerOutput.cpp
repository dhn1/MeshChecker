
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

bool MeshCheckerOutput::verboseReport () const
{
    return m_verboseReport;
}

void MeshCheckerOutput::setVerboseReport (bool newVerboseReport)
{
    m_verboseReport = newVerboseReport;
}

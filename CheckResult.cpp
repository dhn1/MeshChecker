
#include "CheckResult.h"

#include "MeshChecker.h"

CheckResult::CheckResult (Checks check, const QString& path, bool pass, const QString& result, int badCount) : m_path (path), m_pass (pass), m_report (result), m_badCount (badCount), m_check (check)
{
}

QString CheckResult::name () const
{
    return MeshChecker::checkName (m_check);
}

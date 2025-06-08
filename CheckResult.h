#ifndef CHECKRESULT_H
#define CHECKRESULT_H

#include <QString>

class CheckResult
{
public:
    CheckResult (const QString& path, bool pass, const QString& result, int badCount) : m_path (path), m_pass (pass), m_report (result), m_badCount (badCount) {}
    QString m_path;
    bool m_pass;
    QString m_report;
    int m_badCount{};
};

#endif  // CHECKRESULT_H

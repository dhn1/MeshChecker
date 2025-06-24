#ifndef CHECKRESULT_H
#define CHECKRESULT_H

#include <QString>

#include "globals.h"

class CheckResult
{
public:
    CheckResult (Checks check, const QString& path, bool pass, const QString& result, int badCount);

    QString name () const;
    QString m_path;
    bool m_pass;
    QString m_report;
    int m_badCount{};
    Checks m_check;
};

#endif  // CHECKRESULT_H

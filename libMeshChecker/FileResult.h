#ifndef FILERESULT_H
#define FILERESULT_H

#include <QList>

#include "CheckResult.h"

class FileResult
{
public:
    FileResult (const QString& path) : m_path (path) {}
    bool pass () const { return m_pass; }
    void setPass (bool newPass) { m_pass = newPass; }
    QString path () const { return m_path; }
    QList<CheckResult>& checkResults () { return m_checkResults; }
    QList<CheckResult> checkResults () const { return m_checkResults; }

private:
    QList<CheckResult> m_checkResults;
    QString m_path;
    bool m_pass{};
};

#endif  // FILERESULT_H

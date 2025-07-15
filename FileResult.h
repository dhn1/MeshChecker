#ifndef FILERESULT_H
#define FILERESULT_H

#include <QList>

#include "CheckResult.h"

class FileResult
{
public:
    QList<CheckResult> m_checkResults;
    QString m_path;
    bool m_pass{};
};

#endif  // FILERESULT_H

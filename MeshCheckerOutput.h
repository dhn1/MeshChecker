#ifndef MESHCHECKEROUTPUT_H
#define MESHCHECKEROUTPUT_H

#include <Types.h>

#include <QDebug>

#include "MeshChecker.h"

class MeshCheckerOutput : public MeshChecker
{
public:
    MeshCheckerOutput (const MeshPtr& mesh) : MeshChecker (mesh)
    {
    }

    bool verboseReport () const;
    void setVerboseReport (bool newVerboseReport);

    bool quiet () const;
    void setQuiet (bool newQuiet);

protected:
    void report (const QString& str) override
    {
        if (!m_quiet)
        {
            qDebug ().nospace ().noquote () << str;
        }
    }
    void verbose (const QString& str) override
    {
        if (m_verboseReport)
        {
            qDebug ().nospace ().noquote () << str;
        }
    }

private:
    void setVerbose (bool newVerbose);
    bool m_verboseReport{};
    bool m_quiet {false};
};

#endif  // MESHCHECKEROUTPUT_H

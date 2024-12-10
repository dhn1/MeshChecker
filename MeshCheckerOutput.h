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
    void report (const QString& str) override;
    void verbose (const QString& str) override;

private:
    void setVerbose (bool newVerbose);
    bool m_verboseReport{};
    bool m_quiet {false};
};

#endif  // MESHCHECKEROUTPUT_H

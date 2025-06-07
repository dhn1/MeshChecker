#pragma once

#include <QTextStream>

extern QTextStream out;
enum Verbosity
{
    Mute        = 0,
    FileName    = 1,
    Summary     = 2,
    Details     = 4,
};

extern Verbosity verbosity;

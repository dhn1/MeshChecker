#pragma once

#include <QTextStream>

class CheckResult;

extern QTextStream out;

enum Verbosity
{
    Mute        = 0,
    FileName    = 1,
    Summary     = 2,
    Details     = 4,
};

enum Checks {
    CheckNothing                = 0,
    CheckHoles                  = 1 << 0,
    CheckDuplicateTriangles     = 1 << 2,
    CheckShortEdges             = 1 << 3,
    CheckInfo                   = 1 << 4,
    CheckReversedTriangles      = 1 << 5,
    CheckDuplicateVertices      = 1 << 6,
    CheckOpenEdges              = 1 << 7,
    CheckTriangleOverlap        = 1 << 8,
    CheckUnviableTriangles      = 1 << 9,
    CheckFlatTriangles          = 1 << 10,
    CheckOverusedHalfEdges      = 1 << 11,
    CheckDeleted                = 1 << 12,
    CheckVertexLowRefs          = 1 << 13,
    CheckTCount                 = 1 << 14,
    CheckDuplicateAnnotations   = 1 << 15,

    //MultiThread                 = 1 << 18,

    Default = CheckHoles | CheckDuplicateTriangles | CheckShortEdges | CheckInfo | CheckReversedTriangles | CheckDuplicateVertices | CheckOpenEdges | CheckTriangleOverlap | CheckUnviableTriangles | CheckOverusedHalfEdges | CheckFlatTriangles | CheckDeleted | CheckVertexLowRefs | CheckDuplicateAnnotations,
    All = CheckHoles | CheckDuplicateTriangles | CheckShortEdges | CheckInfo | CheckReversedTriangles | CheckDuplicateVertices | CheckOpenEdges | CheckTriangleOverlap | CheckUnviableTriangles | CheckOverusedHalfEdges | CheckFlatTriangles | CheckDeleted | CheckVertexLowRefs | CheckTCount | CheckDuplicateAnnotations,
    Critical = CheckHoles | CheckDuplicateTriangles | CheckReversedTriangles | CheckDuplicateVertices | CheckOpenEdges | CheckTriangleOverlap | CheckOverusedHalfEdges | CheckDeleted | CheckVertexLowRefs,
};

constexpr int padding = 20;


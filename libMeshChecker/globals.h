#pragma once

#include <QTextStream>

class CheckResult;

extern QTextStream out;

enum Verbosity : uint8_t
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
    CheckReversedEdges      = 1 << 5,
    CheckDuplicateVertices      = 1 << 6,
    CheckOpenEdges              = 1 << 7,
    CheckOverlappingTriangles   = 1 << 8,
    CheckUnviableTriangles      = 1 << 9,
    CheckFlatTriangles          = 1 << 10,
    CheckOverusedHalfEdges      = 1 << 11,
    CheckDeleted                = 1 << 12,
    CheckVertexLowRefs          = 1 << 13,
    CheckTCount                 = 1 << 14,
    CheckDuplicateAnnotations   = 1 << 15,
    CheckComponents             = 1 << 16,
    CheckBadlyFormedTriangles   = 1 << 17,
    CheckPockets                = 1 << 18,

    CmpDefault = CheckHoles | CheckDuplicateTriangles | CheckShortEdges | CheckReversedEdges | CheckDuplicateVertices | CheckOpenEdges | CheckOverlappingTriangles | CheckUnviableTriangles | CheckOverusedHalfEdges | CheckFlatTriangles | CheckDeleted | CheckVertexLowRefs | CheckDuplicateAnnotations | CheckBadlyFormedTriangles | CheckPockets,
    Default = CheckHoles | CheckDuplicateTriangles | CheckShortEdges | CheckReversedEdges | CheckDuplicateVertices | CheckOpenEdges | CheckOverlappingTriangles | CheckUnviableTriangles | CheckOverusedHalfEdges | CheckFlatTriangles | CheckDeleted | CheckVertexLowRefs | CheckDuplicateAnnotations | CheckBadlyFormedTriangles | CheckInfo | CheckPockets,
    All = CheckHoles | CheckDuplicateTriangles | CheckShortEdges | CheckInfo | CheckReversedEdges | CheckDuplicateVertices | CheckOpenEdges | CheckOverlappingTriangles | CheckUnviableTriangles | CheckOverusedHalfEdges | CheckFlatTriangles | CheckDeleted | CheckVertexLowRefs | CheckTCount | CheckDuplicateAnnotations | CheckComponents | CheckBadlyFormedTriangles| CheckPockets,
    Critical = CheckHoles | CheckDuplicateTriangles | CheckReversedEdges | CheckDuplicateVertices | CheckOpenEdges | CheckOverlappingTriangles | CheckOverusedHalfEdges | CheckDeleted | CheckVertexLowRefs | CheckBadlyFormedTriangles | CheckPockets,
};

constexpr int padding = 20;

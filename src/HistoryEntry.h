#pragma once

#include "MeshObject.h"
#include <QString>

struct HistoryEntry {
    enum class Type { LoadMesh, ImportMesh, Cut, FlipNormals, UnitChange, Transform };
    Type       type  = Type::LoadMesh;
    QString    label; // short display label (fits under icon)
    MeshObject mesh;  // full mesh snapshot for rollback
};

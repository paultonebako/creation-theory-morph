#pragma once

#include <QString>

class MeshObject;

class ObjLoader
{
public:
    static bool load(const QString& path, MeshObject& mesh, QString* errorMessage = nullptr);
};

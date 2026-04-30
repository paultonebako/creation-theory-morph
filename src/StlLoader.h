#pragma once

#include <QString>

class MeshObject;

class StlLoader
{
public:
    static bool load(const QString& path, MeshObject& mesh, QString* errorMessage = nullptr);

private:
    static bool loadBinary(const QString& path, MeshObject& mesh, QString* errorMessage);
    static bool loadAscii(const QString& path, MeshObject& mesh, QString* errorMessage);
};

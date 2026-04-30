#include "StlLoader.h"

#include "MeshObject.h"

#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QTextStream>

#include <cstring>
#include <cstdint>

namespace {
float readFloatLE(const char* p)
{
    float f;
    std::memcpy(&f, p, sizeof(float));
    return f;
}

std::uint32_t readU32LE(const char* p)
{
    std::uint32_t x;
    std::memcpy(&x, p, sizeof(std::uint32_t));
    return x;
}
}

bool StlLoader::load(const QString& path, MeshObject& mesh, QString* errorMessage)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (errorMessage) {
            *errorMessage = QString("Failed to open STL: %1").arg(file.errorString());
        }
        return false;
    }
    const QByteArray data = file.readAll();
    if (data.size() < 84) {
        return loadAscii(path, mesh, errorMessage);
    }

    const std::uint32_t triCount = readU32LE(data.constData() + 80);
    const qint64 expectedSize = 84 + static_cast<qint64>(triCount) * 50;
    if (expectedSize == data.size()) {
        return loadBinary(path, mesh, errorMessage);
    }
    return loadAscii(path, mesh, errorMessage);
}

bool StlLoader::loadBinary(const QString& path, MeshObject& mesh, QString* errorMessage)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (errorMessage) {
            *errorMessage = QString("Failed to open binary STL: %1").arg(file.errorString());
        }
        return false;
    }

    const QByteArray data = file.readAll();
    if (data.size() < 84) {
        if (errorMessage) {
            *errorMessage = "Binary STL is too small.";
        }
        return false;
    }

    const std::uint32_t triCount = readU32LE(data.constData() + 80);
    const qint64 expectedSize = 84 + static_cast<qint64>(triCount) * 50;
    if (expectedSize != data.size()) {
        if (errorMessage) {
            *errorMessage = "Binary STL size mismatch.";
        }
        return false;
    }

    mesh.clear();
    const char* ptr = data.constData() + 84;
    for (std::uint32_t i = 0; i < triCount; ++i) {
        ptr += 12; // skip normal
        QVector3D v[3];
        for (int k = 0; k < 3; ++k) {
            v[k] = QVector3D(readFloatLE(ptr), readFloatLE(ptr + 4), readFloatLE(ptr + 8));
            ptr += 12;
        }
        const int i0 = mesh.addVertex(v[0]);
        const int i1 = mesh.addVertex(v[1]);
        const int i2 = mesh.addVertex(v[2]);
        mesh.addTriangle(i0, i1, i2);
        ptr += 2; // attribute bytes
    }
    mesh.recomputeNormals();
    return true;
}

bool StlLoader::loadAscii(const QString& path, MeshObject& mesh, QString* errorMessage)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (errorMessage) {
            *errorMessage = QString("Failed to open ASCII STL: %1").arg(file.errorString());
        }
        return false;
    }

    QTextStream in(&file);
    QRegularExpression vertexRx(
        R"(^\s*vertex\s+([+-]?\d*\.?\d+(?:[eE][+-]?\d+)?)\s+([+-]?\d*\.?\d+(?:[eE][+-]?\d+)?)\s+([+-]?\d*\.?\d+(?:[eE][+-]?\d+)?)\s*$)");

    mesh.clear();
    QVector<QVector3D> triVerts;
    triVerts.reserve(3);

    while (!in.atEnd()) {
        const QString line = in.readLine();
        const QRegularExpressionMatch m = vertexRx.match(line);
        if (!m.hasMatch()) {
            continue;
        }

        triVerts.push_back(QVector3D(m.captured(1).toFloat(), m.captured(2).toFloat(), m.captured(3).toFloat()));
        if (triVerts.size() == 3) {
            const int i0 = mesh.addVertex(triVerts[0]);
            const int i1 = mesh.addVertex(triVerts[1]);
            const int i2 = mesh.addVertex(triVerts[2]);
            mesh.addTriangle(i0, i1, i2);
            triVerts.clear();
        }
    }

    if (mesh.triangles().isEmpty()) {
        if (errorMessage) {
            *errorMessage = QString("No facets found in STL: %1").arg(QFileInfo(path).fileName());
        }
        return false;
    }

    mesh.recomputeNormals();
    return true;
}

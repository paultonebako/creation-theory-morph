#include "ObjLoader.h"

#include "MeshObject.h"

#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QStringList>

namespace {
int parseObjIndex(const QString& token, int vertexCount)
{
    const QString first = token.split('/').value(0);
    bool ok = false;
    int idx = first.toInt(&ok);
    if (!ok || idx == 0) {
        return -1;
    }
    if (idx < 0) {
        idx = vertexCount + idx;
    } else {
        idx -= 1;
    }
    return idx;
}
}

bool ObjLoader::load(const QString& path, MeshObject& mesh, QString* errorMessage)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (errorMessage) {
            *errorMessage = QString("Failed to open OBJ: %1").arg(file.errorString());
        }
        return false;
    }

    mesh.clear();
    QTextStream in(&file);

    while (!in.atEnd()) {
        const QString raw = in.readLine().trimmed();
        if (raw.isEmpty() || raw.startsWith('#')) {
            continue;
        }
        const QStringList parts = raw.split(' ', Qt::SkipEmptyParts);
        if (parts.isEmpty()) {
            continue;
        }

        if (parts[0] == "v" && parts.size() >= 4) {
            mesh.addVertex(QVector3D(parts[1].toFloat(), parts[2].toFloat(), parts[3].toFloat()));
        } else if (parts[0] == "f" && parts.size() >= 4) {
            QVector<int> faceIndices;
            faceIndices.reserve(parts.size() - 1);
            for (int i = 1; i < parts.size(); ++i) {
                const int idx = parseObjIndex(parts[i], mesh.vertices().size());
                if (idx < 0 || idx >= mesh.vertices().size()) {
                    if (errorMessage) {
                        *errorMessage = QString("Invalid OBJ face index in %1").arg(QFileInfo(path).fileName());
                    }
                    return false;
                }
                faceIndices.push_back(idx);
            }

            for (int i = 1; i < faceIndices.size() - 1; ++i) {
                mesh.addTriangle(faceIndices[0], faceIndices[i], faceIndices[i + 1]);
            }
        }
    }

    if (mesh.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QString("No geometry found in OBJ: %1").arg(QFileInfo(path).fileName());
        }
        return false;
    }

    mesh.recomputeNormals();
    return true;
}

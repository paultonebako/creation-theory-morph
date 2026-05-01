#pragma once

#include <QMatrix4x4>
#include <QVector>
#include <QVector3D>

class MeshObject
{
public:
    struct Triangle
    {
        int v0 = 0;
        int v1 = 0;
        int v2 = 0;
    };

    void clear();
    bool isEmpty() const;

    int addVertex(const QVector3D& v);
    void addTriangle(int v0, int v1, int v2);

    void recomputeNormals();
    void flipNormals();
    void merge(const MeshObject& other);
    void applyMatrix(const QMatrix4x4& mat);

    /** Multiply every vertex position by s (normals unchanged in direction). */
    void uniformScale(float s);
    /** Move mesh so min Y = 0 and center X/Z at origin (sits on ground grid). */
    void layOnGroundCenterXZ();

    QVector3D minBounds() const;
    QVector3D maxBounds() const;
    QVector3D center() const;
    float radius() const;

    const QVector<QVector3D>& vertices() const { return m_vertices; }
    const QVector<Triangle>& triangles() const { return m_triangles; }
    const QVector<QVector3D>& faceNormals() const { return m_faceNormals; }
    const QVector<QVector3D>& vertexNormals() const { return m_vertexNormals; }

private:
    QVector<QVector3D> m_vertices;
    QVector<Triangle> m_triangles;
    QVector<QVector3D> m_faceNormals;
    QVector<QVector3D> m_vertexNormals;
};

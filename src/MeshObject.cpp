#include "MeshObject.h"

#include <QtMath>

void MeshObject::clear()
{
    m_vertices.clear();
    m_triangles.clear();
    m_faceNormals.clear();
}

bool MeshObject::isEmpty() const
{
    return m_vertices.isEmpty() || m_triangles.isEmpty();
}

int MeshObject::addVertex(const QVector3D& v)
{
    m_vertices.push_back(v);
    return m_vertices.size() - 1;
}

void MeshObject::addTriangle(int v0, int v1, int v2)
{
    m_triangles.push_back({v0, v1, v2});
}

void MeshObject::recomputeNormals()
{
    m_faceNormals.clear();
    m_faceNormals.reserve(m_triangles.size());

    for (const Triangle& tri : m_triangles) {
        const QVector3D& a = m_vertices[tri.v0];
        const QVector3D& b = m_vertices[tri.v1];
        const QVector3D& c = m_vertices[tri.v2];
        QVector3D n = QVector3D::crossProduct(b - a, c - a);
        if (!qFuzzyIsNull(n.lengthSquared())) {
            n.normalize();
        }
        m_faceNormals.push_back(n);
    }
}

void MeshObject::flipNormals()
{
    for (Triangle& tri : m_triangles) {
        qSwap(tri.v1, tri.v2);
    }
    recomputeNormals();
}

void MeshObject::merge(const MeshObject& other)
{
    const int offset = m_vertices.size();
    m_vertices += other.vertices();

    for (const Triangle& tri : other.triangles()) {
        m_triangles.push_back({tri.v0 + offset, tri.v1 + offset, tri.v2 + offset});
    }
    recomputeNormals();
}

void MeshObject::uniformScale(float s)
{
    if (qFuzzyIsNull(s) || s == 1.0f) {
        return;
    }
    for (QVector3D& v : m_vertices) {
        v *= s;
    }
    recomputeNormals();
}

void MeshObject::layOnGroundCenterXZ()
{
    if (isEmpty()) {
        return;
    }
    const QVector3D mn = minBounds();
    const QVector3D mx = maxBounds();
    const QVector3D t(-0.5f * (mn.x() + mx.x()), -mn.y(), -0.5f * (mn.z() + mx.z()));
    for (QVector3D& v : m_vertices) {
        v += t;
    }
    recomputeNormals();
}

QVector3D MeshObject::minBounds() const
{
    if (m_vertices.isEmpty()) {
        return {};
    }
    QVector3D minV = m_vertices.first();
    for (const QVector3D& v : m_vertices) {
        minV.setX(qMin(minV.x(), v.x()));
        minV.setY(qMin(minV.y(), v.y()));
        minV.setZ(qMin(minV.z(), v.z()));
    }
    return minV;
}

QVector3D MeshObject::maxBounds() const
{
    if (m_vertices.isEmpty()) {
        return {};
    }
    QVector3D maxV = m_vertices.first();
    for (const QVector3D& v : m_vertices) {
        maxV.setX(qMax(maxV.x(), v.x()));
        maxV.setY(qMax(maxV.y(), v.y()));
        maxV.setZ(qMax(maxV.z(), v.z()));
    }
    return maxV;
}

QVector3D MeshObject::center() const
{
    return (minBounds() + maxBounds()) * 0.5f;
}

float MeshObject::radius() const
{
    if (m_vertices.isEmpty()) {
        return 1.0f;
    }
    const QVector3D c = center();
    float r2 = 0.0f;
    for (const QVector3D& v : m_vertices) {
        r2 = qMax(r2, (v - c).lengthSquared());
    }
    return qSqrt(r2);
}

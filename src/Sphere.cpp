#include "Sphere.h"

namespace DXEngine {

    Sphere::Sphere(const int SPHERE_RESOLUTION = 4)
        :
        SPHERE_RESOLUTION(SPHERE_RESOLUTION)
    {

        using namespace DirectX;

        std::vector<Vertex> vertices;
        std::vector<unsigned short> indices;

        for (int lat = 0; lat <= SPHERE_RESOLUTION; ++lat) {
            float theta = lat * DirectX::XM_PI / SPHERE_RESOLUTION;
            float sinTheta = sinf(theta);
            float cosTheta = cosf(theta);

            for (int lon = 0; lon <= SPHERE_RESOLUTION; ++lon) {
                float phi = lon * 2 * DirectX::XM_PI / SPHERE_RESOLUTION;
                float sinPhi = sinf(phi);
                float cosPhi = cosf(phi);

                float x = sinPhi * sinTheta;
                float y = cosTheta;
                float z = cosPhi * sinTheta;

                Vertex vertex;
                vertex.pos = DirectX::XMFLOAT3(x, y, z);

                // Calculate normalized norDirectX::mal (same as position after normalization)
                DirectX::XMVECTOR normal = DirectX::XMVectorSet(x, y, z, 0.0f);
                normal = DirectX::XMVector3Normalize(normal);
                DirectX::XMStoreFloat3(&vertex.normal, normal);

                // Calculate texture coordinates (u, v) from latitude and longitude
                float u = phi / (2 * DirectX::XM_PI);
                float v = theta / DirectX::XM_PI;
                vertex.texCoord = DirectX::XMFLOAT2(u, v);

                vertices.push_back(vertex);
            }
        }

        //   for (auto& vertex : vertices) {
        //       vertex.normal.x *= -1.0f;
        //       vertex.normal.y *= -1.0f;      //flip the normals
        //       vertex.normal.z *= -1.0f;
        //   }
        for (int lat = 0; lat < SPHERE_RESOLUTION; ++lat) {
            for (int lon = 0; lon < SPHERE_RESOLUTION; ++lon) {
                int current = lat * (SPHERE_RESOLUTION + 1) + lon;
                int next = current + 1;
                int below = current + SPHERE_RESOLUTION + 1;
                int belowNext = below + 1;

                // Add two triangles for each quad
                indices.push_back(current);
                indices.push_back(below);
                indices.push_back(next);

                indices.push_back(next);
                indices.push_back(below);
                indices.push_back(belowNext);
            }
        }
        for (int i = 0; i < indices.size(); i += 3) {
            std::swap(indices[i], indices[i + 2]);
        }

        // 1) Allocate temporary tangent accumulators
        size_t vCount = vertices.size();
        std::vector<DirectX::XMFLOAT3> tan1(vCount, DirectX::XMFLOAT3(0, 0, 0));
        std::vector<DirectX::XMFLOAT3> tan2(vCount, DirectX::XMFLOAT3(0, 0, 0));

        // 2) Loop over every triangle and accumulate sdir/tdir
        for (size_t i = 0; i < indices.size(); i += 3)
        {
            unsigned i0 = indices[i];
            unsigned i1 = indices[i + 1];
            unsigned i2 = indices[i + 2];

            const DirectX::XMFLOAT3& p0 = vertices[i0].pos;
            const DirectX::XMFLOAT3& p1 = vertices[i1].pos;
            const DirectX::XMFLOAT3& p2 = vertices[i2].pos;

            const DirectX::XMFLOAT2& uv0 = vertices[i0].texCoord;
            const DirectX::XMFLOAT2& uv1 = vertices[i1].texCoord;
            const DirectX::XMFLOAT2& uv2 = vertices[i2].texCoord;

            // Position deltas
            DirectX::XMVECTOR edge1 = DirectX::XMVectorSet(p1.x - p0.x, p1.y - p0.y, p1.z - p0.z, 0);
            DirectX::XMVECTOR edge2 = DirectX::XMVectorSet(p2.x - p0.x, p2.y - p0.y, p2.z - p0.z, 0);

            // UV deltas
            float dU1 = uv1.x - uv0.x;
            float dV1 = uv1.y - uv0.y;
            float dU2 = uv2.x - uv0.x;
            float dV2 = uv2.y - uv0.y;

            float r = 1.0f / (dU1 * dV2 - dU2 * dV1);

            DirectX::XMVECTOR sdir = (edge1 * dV2 - edge2 * dV1) * r;
            DirectX::XMVECTOR tdir = (edge2 * dU1 - edge1 * dU2) * r;

            DirectX::XMFLOAT3 sd, td;
            DirectX::XMStoreFloat3(&sd, sdir);
            DirectX::XMStoreFloat3(&td, tdir);

            // Accumulate for each of the triangle’s vertices
            auto accumulate = [&](unsigned idx, const DirectX::XMFLOAT3& f) {
                tan1[idx].x += f.x; tan1[idx].y += f.y; tan1[idx].z += f.z;
                tan2[idx].x += td.x; tan2[idx].y += td.y; tan2[idx].z += td.z;
                };
            accumulate(i0, sd);
            accumulate(i1, sd);
            accumulate(i2, sd);
            accumulate(i0, td);
            accumulate(i1, td);
            accumulate(i2, td);
        }

        // 3) Orthonormalize per-vertex tangents against normals
        for (size_t i = 0; i < vCount; ++i)
        {
            DirectX::XMVECTOR n = DirectX::XMLoadFloat3(&vertices[i].normal);
            DirectX::XMVECTOR t = DirectX::XMLoadFloat3(&tan1[i]);
            DirectX::XMVECTOR b = DirectX::XMLoadFloat3(&tan2[i]);

            // Gram-Schmidt orthogonalize T against N
            DirectX::XMVECTOR T = DirectX::XMVector3Normalize(t - n * DirectX::XMVector3Dot(n, t));

            // Calculate handedness: bitangent should point in the same direction as cross(N, T) * handedness
            float w = (DirectX::XMVectorGetX(DirectX::XMVector3Dot(DirectX::XMVector3Cross(n, T), b)) < 0.0f) ? -1.0f : 1.0f;

            // Store as float4
            DirectX::XMFLOAT3 tangentVec;
            DirectX::XMStoreFloat3(&tangentVec, T);
            vertices[i].tangent = DirectX::XMFLOAT4(tangentVec.x, tangentVec.y, tangentVec.z, w);
        }

        m_Resource = std::make_shared<MeshResource>(std::move(vertices), std::move(indices));

    }

    Sphere::Sphere(const int resolution, int slot)
        :
        SPHERE_RESOLUTION(resolution)
    {

        using namespace DirectX;

        std::vector<Vertex> vertices;
        std::vector<unsigned short> indices;

        for (int lat = 0; lat <= SPHERE_RESOLUTION; ++lat)
        {
            float theta = lat * DirectX::XM_PI / SPHERE_RESOLUTION;
            float sinTheta = sinf(theta);
            float cosTheta = cosf(theta);

            for (int lon = 0; lon <= SPHERE_RESOLUTION; ++lon)
            {
                float phi = lon * 2 * DirectX::XM_PI / SPHERE_RESOLUTION;
                float sinPhi = sinf(phi);
                float cosPhi = cosf(phi);

                float x = sinPhi * sinTheta;
                float y = cosTheta;
                float z = cosPhi * sinTheta;

                Vertex vertex;
                vertex.pos = DirectX::XMFLOAT3(x, y, z);
                vertices.push_back(vertex);
            }
        }

        for (int lat = 0; lat < SPHERE_RESOLUTION; ++lat)
        {
            for (int lon = 0; lon < SPHERE_RESOLUTION; ++lon)
            {
                int current = lat * (SPHERE_RESOLUTION + 1) + lon;
                int next = current + 1;
                int below = current + SPHERE_RESOLUTION + 1;
                int belowNext = below + 1;

                // Add two triangles for each quad
                indices.push_back(current);
                indices.push_back(below);
                indices.push_back(next);

                indices.push_back(next);
                indices.push_back(below);
                indices.push_back(belowNext);
            }
        }
        // 1) Allocate temporary tangent accumulators
        size_t vCount = vertices.size();
        std::vector<DirectX::XMFLOAT3> tan1(vCount, DirectX::XMFLOAT3(0, 0, 0));
        std::vector<DirectX::XMFLOAT3> tan2(vCount, DirectX::XMFLOAT3(0, 0, 0));

        // 2) Loop over every triangle and accumulate sdir/tdir
        for (size_t i = 0; i < indices.size(); i += 3)
        {
            unsigned i0 = indices[i];
            unsigned i1 = indices[i + 1];
            unsigned i2 = indices[i + 2];

            const DirectX::XMFLOAT3& p0 = vertices[i0].pos;
            const DirectX::XMFLOAT3& p1 = vertices[i1].pos;
            const DirectX::XMFLOAT3& p2 = vertices[i2].pos;

            const DirectX::XMFLOAT2& uv0 = vertices[i0].texCoord;
            const DirectX::XMFLOAT2& uv1 = vertices[i1].texCoord;
            const DirectX::XMFLOAT2& uv2 = vertices[i2].texCoord;

            // Position deltas
            DirectX::XMVECTOR edge1 = DirectX::XMVectorSet(p1.x - p0.x, p1.y - p0.y, p1.z - p0.z, 0);
            DirectX::XMVECTOR edge2 = DirectX::XMVectorSet(p2.x - p0.x, p2.y - p0.y, p2.z - p0.z, 0);

            // UV deltas
            float dU1 = uv1.x - uv0.x;
            float dV1 = uv1.y - uv0.y;
            float dU2 = uv2.x - uv0.x;
            float dV2 = uv2.y - uv0.y;

            float r = 1.0f / (dU1 * dV2 - dU2 * dV1);

            DirectX::XMVECTOR sdir = (edge1 * dV2 - edge2 * dV1) * r;
            DirectX::XMVECTOR tdir = (edge2 * dU1 - edge1 * dU2) * r;

            DirectX::XMFLOAT3 sd, td;
            DirectX::XMStoreFloat3(&sd, sdir);
            DirectX::XMStoreFloat3(&td, tdir);

            // Accumulate for each of the triangle’s vertices
            auto accumulate = [&](unsigned idx, const DirectX::XMFLOAT3& f) {
                tan1[idx].x += f.x; tan1[idx].y += f.y; tan1[idx].z += f.z;
                tan2[idx].x += td.x; tan2[idx].y += td.y; tan2[idx].z += td.z;
                };
            accumulate(i0, sd);
            accumulate(i1, sd);
            accumulate(i2, sd);
            accumulate(i0, td);
            accumulate(i1, td);
            accumulate(i2, td);
        }

        // 3) Orthonormalize per-vertex tangents against normals
        for (size_t i = 0; i < vCount; ++i)
        {
            DirectX::XMVECTOR n = DirectX::XMLoadFloat3(&vertices[i].normal);
            DirectX::XMVECTOR t = DirectX::XMLoadFloat3(&tan1[i]);
            DirectX::XMVECTOR b = DirectX::XMLoadFloat3(&tan2[i]);

            // Gram-Schmidt orthogonalize T against N
            DirectX::XMVECTOR T = DirectX::XMVector3Normalize(t - n * DirectX::XMVector3Dot(n, t));

            // Calculate handedness: bitangent should point in the same direction as cross(N, T) * handedness
            float w = (DirectX::XMVectorGetX(DirectX::XMVector3Dot(DirectX::XMVector3Cross(n, T), b)) < 0.0f) ? -1.0f : 1.0f;

            // Store as float4
            DirectX::XMFLOAT3 tangentVec;
            DirectX::XMStoreFloat3(&tangentVec, T);
            vertices[i].tangent = DirectX::XMFLOAT4(tangentVec.x, tangentVec.y, tangentVec.z, w);
        }

        m_Resource = std::make_shared<MeshResource>(std::move(vertices), std::move(indices));

    }

    Sphere::Sphere(const char* name, int res)
        :
        SPHERE_RESOLUTION(res)
    {
        using namespace DirectX;
        std::vector<Vertex> vertices;
        std::vector<unsigned short> indices;

        for (int lat = 0; lat <= SPHERE_RESOLUTION; ++lat)
        {
            float theta = lat * DirectX::XM_PI / SPHERE_RESOLUTION;
            float sinTheta = sinf(theta);
            float cosTheta = cosf(theta);

            for (int lon = 0; lon <= SPHERE_RESOLUTION; ++lon)
            {
                float phi = lon * 2 * DirectX::XM_PI / SPHERE_RESOLUTION;
                float sinPhi = sinf(phi);
                float cosPhi = cosf(phi);

                float x = sinPhi * sinTheta;
                float y = cosTheta;
                float z = cosPhi * sinTheta;

                Vertex vertex;
                vertex.pos = DirectX::XMFLOAT3(x, y, z);
                vertices.push_back(vertex);
            }
        }

        for (int lat = 0; lat < SPHERE_RESOLUTION; ++lat)
        {
            for (int lon = 0; lon < SPHERE_RESOLUTION; ++lon)
            {
                int current = lat * (SPHERE_RESOLUTION + 1) + lon;
                int next = current + 1;
                int below = current + SPHERE_RESOLUTION + 1;
                int belowNext = below + 1;

                // Add two triangles for each quad
                indices.push_back(current);
                indices.push_back(below);
                indices.push_back(next);

                indices.push_back(next);
                indices.push_back(below);
                indices.push_back(belowNext);
            }
        }

        // 1) Allocate temporary tangent accumulators
        size_t vCount = vertices.size();
        std::vector<DirectX::XMFLOAT3> tan1(vCount, DirectX::XMFLOAT3(0, 0, 0));
        std::vector<DirectX::XMFLOAT3> tan2(vCount, DirectX::XMFLOAT3(0, 0, 0));

        // 2) Loop over every triangle and accumulate sdir/tdir
        for (size_t i = 0; i < indices.size(); i += 3)
        {
            unsigned i0 = indices[i];
            unsigned i1 = indices[i + 1];
            unsigned i2 = indices[i + 2];

            const DirectX::XMFLOAT3& p0 = vertices[i0].pos;
            const DirectX::XMFLOAT3& p1 = vertices[i1].pos;
            const DirectX::XMFLOAT3& p2 = vertices[i2].pos;

            const DirectX::XMFLOAT2& uv0 = vertices[i0].texCoord;
            const DirectX::XMFLOAT2& uv1 = vertices[i1].texCoord;
            const DirectX::XMFLOAT2& uv2 = vertices[i2].texCoord;

            // Position deltas
            DirectX::XMVECTOR edge1 = DirectX::XMVectorSet(p1.x - p0.x, p1.y - p0.y, p1.z - p0.z, 0);
            DirectX::XMVECTOR edge2 = DirectX::XMVectorSet(p2.x - p0.x, p2.y - p0.y, p2.z - p0.z, 0);

            // UV deltas
            float dU1 = uv1.x - uv0.x;
            float dV1 = uv1.y - uv0.y;
            float dU2 = uv2.x - uv0.x;
            float dV2 = uv2.y - uv0.y;

            float r = 1.0f / (dU1 * dV2 - dU2 * dV1);

            DirectX::XMVECTOR sdir = (edge1 * dV2 - edge2 * dV1) * r;
            DirectX::XMVECTOR tdir = (edge2 * dU1 - edge1 * dU2) * r;

            DirectX::XMFLOAT3 sd, td;
            DirectX::XMStoreFloat3(&sd, sdir);
            DirectX::XMStoreFloat3(&td, tdir);

            // Accumulate for each of the triangle’s vertices
            auto accumulate = [&](unsigned idx, const DirectX::XMFLOAT3& f) {
                tan1[idx].x += f.x; tan1[idx].y += f.y; tan1[idx].z += f.z;
                tan2[idx].x += td.x; tan2[idx].y += td.y; tan2[idx].z += td.z;
                };
            accumulate(i0, sd);
            accumulate(i1, sd);
            accumulate(i2, sd);
            accumulate(i0, td);
            accumulate(i1, td);
            accumulate(i2, td);
        }

        // 3) Orthonormalize per-vertex tangents against normals
        for (size_t i = 0; i < vCount; ++i)
        {
            DirectX::XMVECTOR n = DirectX::XMLoadFloat3(&vertices[i].normal);
            DirectX::XMVECTOR t = DirectX::XMLoadFloat3(&tan1[i]);
            DirectX::XMVECTOR b = DirectX::XMLoadFloat3(&tan2[i]);

            // Gram-Schmidt orthogonalize T against N
            DirectX::XMVECTOR T = DirectX::XMVector3Normalize(t - n * DirectX::XMVector3Dot(n, t));

            // Calculate handedness: bitangent should point in the same direction as cross(N, T) * handedness
            float w = (DirectX::XMVectorGetX(DirectX::XMVector3Dot(DirectX::XMVector3Cross(n, T), b)) < 0.0f) ? -1.0f : 1.0f;

            // Store as float4
            DirectX::XMFLOAT3 tangentVec;
            DirectX::XMStoreFloat3(&tangentVec, T);
            vertices[i].tangent = DirectX::XMFLOAT4(tangentVec.x, tangentVec.y, tangentVec.z, w);
        }
        m_Resource = std::make_shared<MeshResource>(std::move(vertices), std::move(indices));

    }

    Sphere::~Sphere()
    {
    }

    std::shared_ptr<MeshResource> Sphere::getMeshResource() const
    {
        return m_Resource;
    }

}



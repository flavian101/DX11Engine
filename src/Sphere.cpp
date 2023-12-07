#include "Sphere.h"


Sphere::Sphere(const int SPHERE_RESOLUTION = 4 ) 
    :
    SPHERE_RESOLUTION(SPHERE_RESOLUTION)
{
   

    for (int lat = 0; lat <= SPHERE_RESOLUTION; ++lat) {
        float theta = lat * XM_PI / SPHERE_RESOLUTION;
        float sinTheta = sinf(theta);
        float cosTheta = cosf(theta);

        for (int lon = 0; lon <= SPHERE_RESOLUTION; ++lon) {
            float phi = lon * 2 * XM_PI / SPHERE_RESOLUTION;
            float sinPhi = sinf(phi);
            float cosPhi = cosf(phi);

            float x = sinPhi * sinTheta;
            float y = cosTheta;
            float z = cosPhi * sinTheta;

            Vertex vertex;
            vertex.pos = XMFLOAT3(x, y, z);

            // Calculate normalized normal (same as position after normalization)
            XMVECTOR normal = XMVectorSet(x, y, z, 0.0f);
            normal = XMVector3Normalize(normal);
            XMStoreFloat3(&vertex.normal, normal);

            // Calculate texture coordinates (u, v) from latitude and longitude
            float u = phi / (2 * XM_PI);
            float v = theta / XM_PI;
            vertex.texCoord = XMFLOAT2(u, v);

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
  
}

Sphere::Sphere(const int resolution, int slot)
    :
    SPHERE_RESOLUTION(resolution)
{

    for (int lat = 0; lat <= SPHERE_RESOLUTION; ++lat)
    {
        float theta = lat * XM_PI / SPHERE_RESOLUTION;
        float sinTheta = sinf(theta);
        float cosTheta = cosf(theta);

        for (int lon = 0; lon <= SPHERE_RESOLUTION; ++lon)
        {
            float phi = lon * 2 * XM_PI / SPHERE_RESOLUTION;
            float sinPhi = sinf(phi);
            float cosPhi = cosf(phi);

            float x = sinPhi * sinTheta;
            float y = cosTheta;
            float z = cosPhi * sinTheta;

            Vertex vertex;
            vertex.pos = XMFLOAT3(x, y, z);
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
}

std::vector<Vertex> Sphere::getVertex()
{
    return vertices;
}

std::vector<unsigned short> Sphere::getIndices()
{
    return indices;
}




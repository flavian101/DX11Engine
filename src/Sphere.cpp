#include "Sphere.h"


Sphere::Sphere(const int SPHERE_RESOLUTION = 4 ) 
    :
    SPHERE_RESOLUTION(SPHERE_RESOLUTION)
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

Vertex* Sphere::getVertex()
{
    return vertices.data();
}

const unsigned short* Sphere::getIndices()
{
    return (const unsigned short*)indices.data();
}

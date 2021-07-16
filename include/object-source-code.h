#pragma once
// object-source-code.h
#include <cmath>
#include <vector>
#include <utility>

namespace objectDictionary
{
    std::pair<std::vector<float>, std::vector<int>> constructSphere(float radius, int latitudeCount, int longitudeCount)
	{
        // sphere equation- x2 + y2 + z2 = r22
        // parameteric form- 
        // x: rCpCt  
        // y: rCpSt 
        // z:rSp

        const float PI = acos(-1.0f);
        int sectorCount = longitudeCount;
        int stackCount = latitudeCount + 1;

        float x, y, z, xy;                              // vertex position
        float nx, ny, nz, lengthInv = 1.0f / radius;    // normal

        float sectorStep = 2.0f * PI / sectorCount;
        float stackStep = PI / stackCount;
        float sectorAngle, stackAngle;

        std::vector<float> vertices;
        std::vector<float> normals;
        std::vector<int> indices;

        int numVertexNormals = (stackCount + 1) * (sectorCount + 1) * 3;
        int numIndices = (stackCount - 1) * sectorCount * 6 + 2 * sectorCount * 3;

        vertices.reserve(numVertexNormals);
        normals.reserve(numVertexNormals);
        indices.reserve(numIndices);

        for (int i = 0; i <= stackCount; ++i)
        {
            stackAngle = PI / 2 - i * stackStep;        // starting from pi/2 to -pi/2
            xy = radius * cosf(stackAngle);             // r * cos(u)
            z = radius * sinf(stackAngle);              // r * sin(u)

            // add (sectorCount+1) vertices per stack
            // the first and last vertices have same position and normal, but different tex coords
            for (int j = 0; j <= sectorCount; ++j)
            {
                sectorAngle = j * sectorStep;           // starting from 0 to 2pi

                // vertex position
                x = xy * cosf(sectorAngle);             // r * cos(u) * cos(v)
                y = xy * sinf(sectorAngle);             // r * cos(u) * sin(v)
                vertices.push_back(x);
                vertices.push_back(y);
                vertices.push_back(z);

                // normalized vertex normal
                nx = x * lengthInv;
                ny = y * lengthInv;
                nz = z * lengthInv;
                normals.push_back(nx);
                normals.push_back(ny);
                normals.push_back(nz);
            }
        }

        // indices
        //  k1--k1+1
        //  |  / |
        //  | /  |
        //  k2--k2+1
        unsigned int k1, k2;
        for (int i = 0; i < stackCount; ++i)
        {
            k1 = i * (sectorCount + 1);     // beginning of current stack
            k2 = k1 + sectorCount + 1;      // beginning of next stack

            for (int j = 0; j < sectorCount; ++j, ++k1, ++k2)
            {
                // 2 triangles per sector excluding 1st and last stacks
                if (i != 0)
                {
                    indices.push_back(k1);
                    indices.push_back(k2);
                    indices.push_back(k1 + 1);
                }

                if (i != (stackCount - 1))
                {
                    indices.push_back(k1 + 1);
                    indices.push_back(k2);
                    indices.push_back(k2 + 1);
                }
            }
        }

        // generate interleaved vertex array as well
        std::vector<float> interleavedVertices;
        std::size_t i, j;
        std::size_t count = vertices.size();
        for (i = 0, j = 0; i < count; i += 3, j += 2)
        {
            interleavedVertices.push_back(vertices[i]);
            interleavedVertices.push_back(vertices[i + 1]);
            interleavedVertices.push_back(vertices[i + 2]);

            interleavedVertices.push_back(normals[i]);
            interleavedVertices.push_back(normals[i + 1]);
            interleavedVertices.push_back(normals[i + 2]);
        }
        return { interleavedVertices,indices };
	}
    
}
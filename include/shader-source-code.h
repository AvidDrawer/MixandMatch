#pragma once
// shader-source-code.h
// Contains the source code for all the shaders used.

namespace primaryshader
{
    const char* vs = R"glsl(
#version 460 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNorm;
layout(location = 2) in vec3 offset;
layout(location = 3) in int render;
layout(location = 4) in float startExplodingTime;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform float currentTime;

out vec3 Posf;
out vec3 Normf;

out VS_OUT{
    vec3 normal;
    vec3 posf;
    float time;
    int discardit;
} vs_out;
void main()
{
    Normf = normalize(mat3(transpose(inverse(model))) * aNorm);
    Posf = vec3(model * vec4(aPos+offset, 1.0f));
    gl_Position = projection * view * model * vec4(aPos+offset, 1.0f);
   
    vs_out.posf = vec3(model * vec4(aPos+offset, 1.0f));
    vs_out.normal = normalize(mat3(transpose(inverse(model))) * aNorm);
    vs_out.time = currentTime - startExplodingTime;
    vs_out.discardit = render;
}
)glsl";

    const char* fs = R"glsl(
#version 460 core
out vec4 FragCol;
uniform int linemode;

in vec3 Posf;
in vec3 Normf;

uniform samplerCube skybox;
uniform vec3 cameraPos;

void main()
{
   if(linemode == 1)
       FragCol = vec4(0.0f,0.0f,0.0f,1.0f);
   else
   {
       vec3 I = normalize(Posf - cameraPos);
       vec3 refDir = reflect(I, Normf);
	   FragCol = vec4(texture(skybox, refDir).rgb,1.0f);
   }
}
)glsl";

    const char* gs = R"glsl(
#version 460 core
layout(triangles) in;
layout(triangle_strip, max_vertices = 3) out;

in VS_OUT{
    vec3 normal;
    vec3 posf;
    float time;
    int discardit;
} gs_in[];

uniform float lifetime;

out vec3 Posf;
out vec3 Normf;
void main()
{
   if(gs_in[0].discardit>1)
   {
       gl_Position = gl_in[0].gl_Position;
       Posf = gs_in[0].posf;
       Normf = gs_in[0].normal;
       EmitVertex();

       gl_Position = gl_in[1].gl_Position;
       Posf = gs_in[1].posf;
       Normf = gs_in[1].normal;
       EmitVertex();

       gl_Position = gl_in[2].gl_Position;
       Posf = gs_in[2].posf;
       Normf = gs_in[2].normal;
       EmitVertex();
       EndPrimitive();
   }
   else
   {
   vec3 v1 = vec3(gl_in[0].gl_Position) - vec3(gl_in[1].gl_Position);
   vec3 v2 = vec3(gl_in[2].gl_Position) - vec3(gl_in[1].gl_Position);
   vec3 n = cross(v1, v2);
   float mag = gs_in[2].time;

   vec4 midpt, movedpos[3];
   movedpos[0] = gl_in[0].gl_Position + vec4(n * mag, 0.0f);
   movedpos[1] = gl_in[1].gl_Position + vec4(n * mag, 0.0f);
   movedpos[2] = gl_in[2].gl_Position + vec4(n * mag, 0.0f);
   midpt = (movedpos[0] + movedpos[1] + movedpos[2]) / 3;
   float f1 = gs_in[0].time / lifetime;
   float f2 = 1 - f1;

   gl_Position = f1 * midpt + f2 * movedpos[0];
   Posf = gs_in[0].posf + vec3(n * mag);
   Normf = gs_in[0].normal;
   EmitVertex();

   gl_Position = f1 * midpt + f2 * movedpos[1];
   Posf = gs_in[1].posf + vec3(n * mag);
   Normf = gs_in[1].normal;
   EmitVertex();

   gl_Position = f1 * midpt + f2 * movedpos[2];
   Posf = gs_in[2].posf + vec3(n * mag);
   Normf = gs_in[2].normal;
   EmitVertex();
   EndPrimitive();
   }
}
)glsl";
}

namespace verifynormalsshader
{
    const char* vs = R"glsl(
#version 460 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNorm;
layout(location = 2) in vec3 offset;
uniform mat4 model;
uniform mat4 view;

out VS_OUT {
 vec3 normal;
}vs_out;

void main()
{
   vs_out.normal = normalize(mat3(transpose(inverse(view * model))) * aNorm);
	gl_Position = view * model * vec4(aPos+offset, 1.0f);
}
)glsl";

    const char* gs = R"glsl(
#version 460 core
layout (triangles) in; 
layout (line_strip, max_vertices = 6) out;
in VS_OUT {
 vec3 normal;
}vs_in[];
uniform mat4 projection;

void GenerateLine(int index)
{
    gl_Position = projection * gl_in[index].gl_Position;
    EmitVertex();
    gl_Position = projection * (gl_in[index].gl_Position + vec4(vs_in[index].normal, 0.0)*10);
    EmitVertex();
    EndPrimitive();
}
void main()
{
    GenerateLine(0); // first vertex normal
    GenerateLine(1); // second vertex normal
    GenerateLine(2); // third vertex normal
}
)glsl";

    const char* fs = R"glsl(
#version 460 core
out vec4 FragCol;

void main()
{
    FragCol = vec4(1.0f,1.0f,0.0f,1.0f);
}
)glsl";
}

namespace cubemapshader
{
    const char* vs = R"glsl(
#version 460 core
layout (location = 0) in vec3 pos;
uniform mat4 view;
uniform mat4 projection;
out vec3 texCoords;
void main()
{
   texCoords = pos;
   vec4 p = projection * view * vec4(pos,1.0f);
   gl_Position = p.xyww;
}
)glsl";

    const char* fs = R"glsl(
#version 460 core
out vec4 FragCol;
in vec3 texCoords;
uniform samplerCube skybox;
void main()
{
	    FragCol = texture(skybox, texCoords);
}
)glsl";
}
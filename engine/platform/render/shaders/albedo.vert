#version 330 core

layout (location = 0) in vec3 pos;
layout (location = 1) in vec2 tex;
layout (location = 2) in vec3 norm;
layout (location = 3) in ivec4 boneIds;
layout (location = 4) in vec4 boneWeights;
layout (location = 5) in vec3 col;

out vec4 vCol;
out vec2 TexCoord;

uniform mat4 model;
uniform mat4 projection;
uniform mat4 view;

const int MAX_BONES = 100;
uniform mat4 gBones[MAX_BONES];
uniform bool useBones;

void main()
{
    vec4 totalPosition = vec4(0.0f);
    
    if (useBones) {
        for (int i = 0; i < 4; i++) {
            if (boneIds[i] == -1) continue;
            if (boneIds[i] >= MAX_BONES) {
                totalPosition = vec4(pos, 1.0f);
                break;
            }
            vec4 localPosition = gBones[boneIds[i]] * vec4(pos, 1.0f);
            totalPosition += localPosition * boneWeights[i];
        }
    } else {
        totalPosition = vec4(pos, 1.0f);
    }
    
    gl_Position = projection * view * model * totalPosition;
    
    // Pass vertex color (used if no texture)
    vCol = vec4(col, 1.0f);
    
    // Pass texture coordinates
    TexCoord = tex;
}


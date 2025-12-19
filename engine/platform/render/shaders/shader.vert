#version 330

layout (location = 0) in vec3 pos;
layout (location = 1) in vec2 tex;
layout (location = 2) in vec3 norm;
// Bone data (if available - will be 0,0,0,0 and 0.0,0.0,0.0,0.0 if not used)
layout (location = 3) in ivec4 boneIDs;      // Bone indices (which bones influence this vertex)
layout (location = 4) in vec4 boneWeights;   // Bone weights (how much each bone influences)
layout (location = 5) in vec3 vertexColor;  // Vertex color (if available)

out vec4 vCol;
out vec2 TexCoord;
out vec3 Normal;
out vec3 FragPos;
out vec4 DirectionalLightSpacePos;

uniform mat4 model;
uniform mat4 projection;
uniform mat4 view;
uniform mat4 directionalLightTransform;

// Bone transformation matrices (for GPU skinning)
// Maximum 100 bones (can be increased if needed)
#define MAX_BONES 100
uniform mat4 boneTransforms[MAX_BONES];
uniform int boneCount;  // Number of bones actually used (0 = no animation)

void main()
{
	vec4 finalPosition = vec4(pos, 1.0);
	vec3 finalNormal = norm;
	
	// Apply GPU skinning if bones are available
	if (boneCount > 0) {
		// Calculate bone transformation matrix by blending influences
		mat4 boneTransform = mat4(0.0);
		
		// Blend bone influences (up to 4 bones per vertex)
		if (boneIDs.x >= 0 && boneIDs.x < MAX_BONES) {
			boneTransform += boneTransforms[boneIDs.x] * boneWeights.x;
		}
		if (boneIDs.y >= 0 && boneIDs.y < MAX_BONES) {
			boneTransform += boneTransforms[boneIDs.y] * boneWeights.y;
		}
		if (boneIDs.z >= 0 && boneIDs.z < MAX_BONES) {
			boneTransform += boneTransforms[boneIDs.z] * boneWeights.z;
		}
		if (boneIDs.w >= 0 && boneIDs.w < MAX_BONES) {
			boneTransform += boneTransforms[boneIDs.w] * boneWeights.w;
		}
		
		// Apply bone transformation to vertex position and normal
		finalPosition = boneTransform * finalPosition;
		finalNormal = mat3(boneTransform) * finalNormal;
	}
	
	// Transform to world space, then view, then projection
	gl_Position = projection * view * model * finalPosition;
	DirectionalLightSpacePos = directionalLightTransform * model * finalPosition;
	
	// Use vertex color if available, otherwise use position-based color
	if (length(vertexColor) > 0.001) {
		vCol = vec4(vertexColor, 1.0f);
	} else {
		vCol = vec4(clamp(pos, 0.0f, 1.0f), 1.0f);
	}
	
	TexCoord = tex;
	
	// Transform normal to world space
	Normal = mat3(transpose(inverse(model))) * finalNormal;
	
	FragPos = (model * finalPosition).xyz; 
}




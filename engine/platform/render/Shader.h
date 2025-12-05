#pragma once

#include <string>
#include <fstream>
#include <GL/glew.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "render/CommonValues.h"

namespace Engine {
namespace Platform {
namespace Render {

class Shader {
public:
    Shader();
    
    void CreateFromString(const char* vertexCode, const char* fragmentCode);
    void CreateFromFiles(const char* vertexLocation, const char* fragmentLocation);
    void CreateFromFiles(const char* vertexLocation, const char* geometryLocation, const char* fragmentLocation);
    
    void Validate();
    
    std::string ReadFile(const char* fileLocation);
    
    // Basic uniform getters (returns -1 if uniform doesn't exist)
    GLint GetProjectionLocation();
    GLint GetModelLocation();
    GLint GetViewLocation();
    GLint GetEyePositionLocation();
    GLint GetSpecularIntensityLocation();
    GLint GetShininessLocation();
    
    // Check if shader is valid
    bool IsValid() const { return shaderID != 0; }
    
    // Get shader program ID (for advanced uniform access)
    GLuint GetShaderID() const { return shaderID; }
    
    // Basic uniform setters
    void SetTexture(GLuint textureUnit);
    void SetDirectionalLightTransform(glm::mat4* lTransform);
    
    void UseShader();
    void ClearShader();
    
    ~Shader();

private:
    GLuint shaderID;
    GLint uniformProjection;  // GLint can be -1 for invalid locations
    GLint uniformModel;
    GLint uniformView;
    GLint uniformEyePosition;
    GLint uniformSpecularIntensity;
    GLint uniformShininess;
    GLint uniformTexture;
    GLint uniformDirectionalLightTransform;
    
    void CompileShader(const char* vertexCode, const char* fragmentCode);
    void CompileShader(const char* vertexCode, const char* geometryCode, const char* fragmentCode);
    void AddShader(GLuint theProgram, const char* shaderCode, GLenum shaderType);
    void CompileProgram();
};

} // namespace Render
} // namespace Platform
} // namespace Engine




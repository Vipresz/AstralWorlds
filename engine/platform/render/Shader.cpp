#include "render/Shader.h"
#include <cstdio>
#include <cstring>
#include <iostream>

namespace Engine {
namespace Platform {
namespace Render {

Shader::Shader() {
    shaderID = 0;
    uniformModel = 0;
    uniformProjection = 0;
    uniformView = 0;
    uniformEyePosition = 0;
    uniformSpecularIntensity = 0;
    uniformShininess = 0;
    uniformTexture = 0;
    uniformDirectionalLightTransform = 0;
}

void Shader::CreateFromString(const char* vertexCode, const char* fragmentCode) {
    CompileShader(vertexCode, fragmentCode);
}

void Shader::CreateFromFiles(const char* vertexLocation, const char* fragmentLocation) {
    std::string vertexString = ReadFile(vertexLocation);
    std::string fragmentString = ReadFile(fragmentLocation);
    const char* vertexCode = vertexString.c_str();
    const char* fragmentCode = fragmentString.c_str();
    
    CompileShader(vertexCode, fragmentCode);
}

void Shader::CreateFromFiles(const char* vertexLocation, const char* geometryLocation, const char* fragmentLocation) {
    std::string vertexString = ReadFile(vertexLocation);
    std::string geometryString = ReadFile(geometryLocation);
    std::string fragmentString = ReadFile(fragmentLocation);
    const char* vertexCode = vertexString.c_str();
    const char* geometryCode = geometryString.c_str();
    const char* fragmentCode = fragmentString.c_str();
    
    CompileShader(vertexCode, geometryCode, fragmentCode);
}

std::string Shader::ReadFile(const char* fileLocation) {
    std::string content;
    std::ifstream fileStream(fileLocation, std::ios::in);
    
    if (!fileStream.is_open()) {
        printf("Failed to read %s! File doesn't exist.\n", fileLocation);
        return "";
    }
    
    std::string line = "";
    while (!fileStream.eof()) {
        std::getline(fileStream, line);
        content.append(line + "\n");
    }
    
    fileStream.close();
    return content;
}

void Shader::CompileShader(const char* vertexCode, const char* fragmentCode) {
    shaderID = glCreateProgram();
    
    if (!shaderID) {
        printf("Error creating shader program!\n");
        return;
    }
    
    AddShader(shaderID, vertexCode, GL_VERTEX_SHADER);
    AddShader(shaderID, fragmentCode, GL_FRAGMENT_SHADER);
    
    CompileProgram();
}

void Shader::CompileShader(const char* vertexCode, const char* geometryCode, const char* fragmentCode) {
    shaderID = glCreateProgram();
    
    if (!shaderID) {
        printf("Error creating shader program!\n");
        return;
    }
    
    AddShader(shaderID, vertexCode, GL_VERTEX_SHADER);
    AddShader(shaderID, geometryCode, GL_GEOMETRY_SHADER);
    AddShader(shaderID, fragmentCode, GL_FRAGMENT_SHADER);
    
    CompileProgram();
}

void Shader::Validate() {
    GLint result = 0;
    GLchar eLog[1024] = { 0 };
    
    glValidateProgram(shaderID);
    glGetProgramiv(shaderID, GL_VALIDATE_STATUS, &result);
    if (!result) {
        glGetProgramInfoLog(shaderID, sizeof(eLog), NULL, eLog);
        printf("Error validating program: '%s'\n", eLog);
        return;
    }
}

void Shader::CompileProgram() {
    GLint result = 0;
    GLchar eLog[1024] = { 0 };
    
    glLinkProgram(shaderID);
    glGetProgramiv(shaderID, GL_LINK_STATUS, &result);
    if (!result) {
        glGetProgramInfoLog(shaderID, sizeof(eLog), NULL, eLog);
        printf("Error linking program: '%s'\n", eLog);
        shaderID = 0; // Mark as invalid
        return;
    }
    
    // Verify program is valid before getting uniform locations
    GLint isValid = 0;
    glGetProgramiv(shaderID, GL_LINK_STATUS, &isValid);
    if (!isValid) {
        printf("Error: Shader program failed to link properly!\n");
        shaderID = 0;
        return;
    }
    
    // Get uniform locations (these can be -1 if uniform doesn't exist or isn't used)
    uniformProjection = glGetUniformLocation(shaderID, "projection");
    uniformModel = glGetUniformLocation(shaderID, "model");
    uniformView = glGetUniformLocation(shaderID, "view");
    uniformEyePosition = glGetUniformLocation(shaderID, "eyePosition");
    uniformSpecularIntensity = glGetUniformLocation(shaderID, "material.specularIntensity");
    uniformShininess = glGetUniformLocation(shaderID, "material.shininess");
    uniformTexture = glGetUniformLocation(shaderID, "theTexture");
    uniformDirectionalLightTransform = glGetUniformLocation(shaderID, "directionalLightTransform");
    
    // Warn about missing critical uniforms (but continue - they might be optional)
    if (uniformProjection == -1) {
        printf("Warning: Uniform 'projection' not found in shader (might be unused)\n");
    }
    if (uniformView == -1) {
        printf("Warning: Uniform 'view' not found in shader (might be unused)\n");
    }
    if (uniformModel == -1) {
        printf("Warning: Uniform 'model' not found in shader (might be unused)\n");
    }
}

GLint Shader::GetProjectionLocation() {
    return uniformProjection;
}

GLint Shader::GetModelLocation() {
    return uniformModel;
}

GLint Shader::GetViewLocation() {
    return uniformView;
}

GLint Shader::GetEyePositionLocation() {
    return uniformEyePosition;
}

GLint Shader::GetSpecularIntensityLocation() {
    return uniformSpecularIntensity;
}

GLint Shader::GetShininessLocation() {
    return uniformShininess;
}

void Shader::SetTexture(GLuint textureUnit) {
    glUniform1i(uniformTexture, textureUnit);
}

void Shader::SetDirectionalLightTransform(glm::mat4* lTransform) {
    if (uniformDirectionalLightTransform != -1) {
        glUniformMatrix4fv(uniformDirectionalLightTransform, 1, GL_FALSE, glm::value_ptr(*lTransform));
    }
}

void Shader::UseShader() {
    if (shaderID == 0) {
        printf("Error: Cannot use shader - shader program is not initialized!\n");
        return;
    }
    glUseProgram(shaderID);
    
    // Check for OpenGL errors
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        printf("Error: OpenGL error when using shader program: 0x%X\n", error);
    }
}

void Shader::ClearShader() {
    if (shaderID != 0) {
        glDeleteProgram(shaderID);
        shaderID = 0;
    }
    
    uniformModel = 0;
    uniformProjection = 0;
    uniformView = 0;
}

void Shader::AddShader(GLuint theProgram, const char* shaderCode, GLenum shaderType) {
    GLuint theShader = glCreateShader(shaderType);
    
    const GLchar* theCode[1];
    theCode[0] = shaderCode;
    
    GLint codeLength[1];
    codeLength[0] = static_cast<GLint>(strlen(shaderCode));
    
    glShaderSource(theShader, 1, theCode, codeLength);
    glCompileShader(theShader);
    
    GLint result = 0;
    GLchar eLog[1024] = { 0 };
    
    glGetShaderiv(theShader, GL_COMPILE_STATUS, &result);
    if (!result) {
        glGetShaderInfoLog(theShader, sizeof(eLog), NULL, eLog);
        printf("Error compiling the %d shader: '%s'\n", shaderType, eLog);
        return;
    }
    
    glAttachShader(theProgram, theShader);
}

Shader::~Shader() {
    ClearShader();
}

} // namespace Render
} // namespace Platform
} // namespace Engine




#include "render/Texture.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <cstdio>

namespace Engine {
namespace Platform {
namespace Render {

Texture::Texture() {
    textureID = 0;
    width = 0;
    height = 0;
    bitDepth = 0;
    fileLocation = "";
}

Texture::Texture(const char* fileLoc) {
    textureID = 0;
    width = 0;
    height = 0;
    bitDepth = 0;
    fileLocation = fileLoc;
}

bool Texture::LoadTexture() {
    unsigned char* texData = stbi_load(fileLocation, &width, &height, &bitDepth, 0);
    if (!texData) {
        printf("Failed to find: %s\n", fileLocation);
        return false;
    }
    
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    GLenum format = (bitDepth == 4) ? GL_RGBA : GL_RGB;
    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, texData);
    glGenerateMipmap(GL_TEXTURE_2D);
    
    glBindTexture(GL_TEXTURE_2D, 0);
    
    stbi_image_free(texData);
    
    return true;
}

bool Texture::LoadTextureA() {
    unsigned char* texData = stbi_load(fileLocation, &width, &height, &bitDepth, 0);
    if (!texData) {
        printf("Failed to find: %s\n", fileLocation);
        return false;
    }
    
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, texData);
    glGenerateMipmap(GL_TEXTURE_2D);
    
    glBindTexture(GL_TEXTURE_2D, 0);
    
    stbi_image_free(texData);
    
    return true;
}

void Texture::UseTexture() {
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, textureID);
}

void Texture::ClearTexture() {
    if (textureID != 0) {
        glDeleteTextures(1, &textureID);
        textureID = 0;
        width = 0;
        height = 0;
        bitDepth = 0;
    }
}

Texture::~Texture() {
    ClearTexture();
}

} // namespace Render
} // namespace Platform
} // namespace Engine




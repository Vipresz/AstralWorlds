#include "Gear360Stream.h"
#include "../../engine/platform/Logger.h"
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <vector>
#include <cstring>

#define STB_IMAGE_IMPLEMENTATION
#include "../../engine/platform/render/stb_image.h"

using namespace Engine::Editors::CameraSystem;
using EngineLogging::Logger;

// Simple shader source for rendering video texture
const char* vertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aTexCoord;

out vec2 TexCoord;

void main() {
    gl_Position = vec4(aPos, 0.0, 1.0);
    TexCoord = aTexCoord;
}
)";

const char* fragmentShaderSource = R"(
#version 330 core
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;

void main() {
    FragColor = texture(videoTexture, TexCoord);
}
)";

// Compile shader
GLuint CompileShader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    
    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, nullptr, infoLog);
        std::cerr << "Shader compilation error: " << infoLog << std::endl;
        glDeleteShader(shader);
        return 0;
    }
    
    return shader;
}

// Create shader program
GLuint CreateShaderProgram() {
    GLuint vertexShader = CompileShader(GL_VERTEX_SHADER, vertexShaderSource);
    GLuint fragmentShader = CompileShader(GL_FRAGMENT_SHADER, fragmentShaderSource);
    
    if (vertexShader == 0 || fragmentShader == 0) {
        return 0;
    }
    
    GLuint program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);
    
    GLint success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(program, 512, nullptr, infoLog);
        std::cerr << "Program linking error: " << infoLog << std::endl;
        glDeleteProgram(program);
        return 0;
    }
    
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    
    return program;
}

// Load JPEG data to OpenGL texture using stb_image
GLuint CreateTextureFromJPEG(const std::vector<uint8_t>& jpegData, int& width, int& height) {
    if (jpegData.empty()) {
        return 0;
    }
    
    int channels;
    unsigned char* imageData = stbi_load_from_memory(
        jpegData.data(), 
        static_cast<int>(jpegData.size()),
        &width, &height, &channels, 0
    );
    
    if (!imageData) {
        std::cerr << "Failed to decode JPEG: " << stbi_failure_reason() << std::endl;
        return 0;
    }
    
    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    
    // Set texture parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    // Determine format based on channels
    GLenum format = GL_RGB;
    if (channels == 4) {
        format = GL_RGBA;
    } else if (channels == 1) {
        format = GL_RED;
    }
    
    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, imageData);
    
    stbi_image_free(imageData);
    glBindTexture(GL_TEXTURE_2D, 0);
    
    return texture;
}

// Update texture with new JPEG data
// Returns true if texture was recreated (size changed)
bool UpdateTextureFromJPEG(GLuint& texture, const std::vector<uint8_t>& jpegData, int& width, int& height) {
    if (jpegData.empty()) {
        return false;
    }
    
    int channels;
    int newWidth, newHeight;
    unsigned char* imageData = stbi_load_from_memory(
        jpegData.data(), 
        static_cast<int>(jpegData.size()),
        &newWidth, &newHeight, &channels, 0
    );
    
    if (!imageData) {
        std::cerr << "Failed to decode JPEG: " << stbi_failure_reason() << std::endl;
        return false;
    }
    
    // Determine format based on channels
    GLenum format = GL_RGB;
    if (channels == 4) {
        format = GL_RGBA;
    } else if (channels == 1) {
        format = GL_RED;
    }
    
    glBindTexture(GL_TEXTURE_2D, texture);
    
    // Check if we need to recreate texture (size changed)
    if (texture == 0 || newWidth != width || newHeight != height) {
        if (texture != 0) {
            glDeleteTextures(1, &texture);
        }
        
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        
        glTexImage2D(GL_TEXTURE_2D, 0, format, newWidth, newHeight, 0, format, GL_UNSIGNED_BYTE, imageData);
        
        width = newWidth;
        height = newHeight;
        
        stbi_image_free(imageData);
        glBindTexture(GL_TEXTURE_2D, 0);
        return true;
    } else {
        // Same size, just update
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, format, GL_UNSIGNED_BYTE, imageData);
        stbi_image_free(imageData);
        glBindTexture(GL_TEXTURE_2D, 0);
        return false;
    }
}

int main(int argc, char* argv[]) {
    // Initialize logger
    EngineLogging::Logger& logger = EngineLogging::Logger::GetInstance();
    logger.Initialize("gear360_test.log");
    logger.SetMinLevel(EngineLogging::LogLevel::LevelInfo);
    
    std::cout << "=== Gear 360 Video Stream Test ===" << std::endl;
    LOG_INFO("Gear360Test", "Starting Gear 360 video stream test");
    
    // Initialize GLFW
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return 1;
    }
    
    // Configure GLFW
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
    
    // Create window
    const int windowWidth = 1280;
    const int windowHeight = 720;
    GLFWwindow* window = glfwCreateWindow(windowWidth, windowHeight, "Gear 360 Stream", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return 1;
    }
    
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);  // VSync
    
    // Initialize GLEW
    if (glewInit() != GLEW_OK) {
        std::cerr << "Failed to initialize GLEW" << std::endl;
        glfwTerminate();
        return 1;
    }
    
    std::cout << "OpenGL Version: " << glGetString(GL_VERSION) << std::endl;
    
    // Create shader program
    GLuint shaderProgram = CreateShaderProgram();
    if (shaderProgram == 0) {
        std::cerr << "Failed to create shader program" << std::endl;
        glfwTerminate();
        return 1;
    }
    
    // Create quad for rendering video
    float quadVertices[] = {
        // Positions   // TexCoords
        -1.0f,  1.0f,  0.0f, 1.0f,
        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
        -1.0f,  1.0f,  0.0f, 1.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
         1.0f,  1.0f,  1.0f, 1.0f
    };
    
    GLuint VAO, VBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
    
    // Position attribute
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    // TexCoord attribute
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    
    glBindVertexArray(0);
    
    // Create texture for video
    GLuint videoTexture = 0;
    int textureWidth = 1920;
    int textureHeight = 960;
    
    // Initialize Gear 360 stream
    auto stream = CreateGear360Stream("Gear 360");
    Gear360Config config;
    config.useWiFi = true;  // Use WiFi for video streaming
    config.wifiSSID = "";   // Will connect to camera's network
    config.videoWidth = 3840;
    config.videoHeight = 1920;
    config.fps = 30;
    
    // Camera IP and stream path - adjust these if needed
    config.cameraIP = "192.168.1.1";  // Try "192.168.43.1" if this doesn't work
    config.streamPath = "/stream";    // Try "/mjpeg", "/video", or "/live" if needed
    
    // HTTP authentication (if required by camera)
    // config.httpUsername = "admin";
    // config.httpPassword = "password";
    
    stream->SetConfig(config);
    
    // Set video frame callback
    stream->SetVideoFrameCallback([&](const VideoFrame& frame) {
        // Update texture with new frame
        if (videoTexture == 0) {
            videoTexture = CreateTextureFromJPEG(frame.data, textureWidth, textureHeight);
        } else {
            UpdateTextureFromJPEG(videoTexture, frame.data, textureWidth, textureHeight);
        }
    });
    
    std::cout << "Connecting to Gear 360..." << std::endl;
    LOG_INFO("Gear360Test", "Connecting to Gear 360...");
    
    if (!stream->Connect()) {
        std::cerr << "Failed to connect to Gear 360" << std::endl;
        std::cerr << "Make sure:" << std::endl;
        std::cerr << "  1. Gear 360 is powered on" << std::endl;
        std::cerr << "  2. You're connected to the Gear 360's WiFi network" << std::endl;
        std::cerr << "  3. The camera is in streaming mode" << std::endl;
        glfwTerminate();
        return 1;
    }
    
    std::cout << "Connected! Starting stream..." << std::endl;
    LOG_INFO("Gear360Test", "Connected, starting stream");
    
    stream->StartStreaming();
    
    // Main loop
    double lastTime = glfwGetTime();
    int frameCount = 0;
    
    while (!glfwWindowShouldClose(window)) {
        // Poll events
        glfwPollEvents();
        
        // Check for ESC key
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            glfwSetWindowShouldClose(window, GL_TRUE);
        }
        
        // Clear screen
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        
        // Render video texture
        if (videoTexture != 0) {
            glUseProgram(shaderProgram);
            glBindTexture(GL_TEXTURE_2D, videoTexture);
            glBindVertexArray(VAO);
            glDrawArrays(GL_TRIANGLES, 0, 6);
            glBindVertexArray(0);
        } else {
            // Show "Waiting for stream..." message
            // (In a real app, you'd render text here)
        }
        
        // Swap buffers
        glfwSwapBuffers(window);
        
        // Update FPS counter
        frameCount++;
        double currentTime = glfwGetTime();
        if (currentTime - lastTime >= 1.0) {
            double fps = frameCount / (currentTime - lastTime);
            std::string title = "Gear 360 Stream - FPS: " + std::to_string(static_cast<int>(fps));
            if (stream->IsStreaming()) {
                title += " | Stream FPS: " + std::to_string(static_cast<int>(stream->GetFPS()));
            }
            glfwSetWindowTitle(window, title.c_str());
            frameCount = 0;
            lastTime = currentTime;
        }
        
        // Poll for video frames (alternative to callback)
        VideoFrame frame;
        if (stream->PollVideoFrame(frame)) {
            // Frame already processed in callback, but we could do additional processing here
        }
    }
    
    // Cleanup
    std::cout << "Stopping stream..." << std::endl;
    stream->StopStreaming();
    stream->Disconnect();
    
    if (videoTexture != 0) {
        glDeleteTextures(1, &videoTexture);
    }
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteProgram(shaderProgram);
    
    glfwTerminate();
    logger.Shutdown();
    
    std::cout << "Exiting..." << std::endl;
    return 0;
}

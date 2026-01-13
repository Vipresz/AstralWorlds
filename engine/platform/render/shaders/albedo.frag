#version 330 core

in vec4 vCol;
in vec2 TexCoord;

out vec4 colour;

uniform sampler2D theTexture;
uniform bool useTexture;

void main()
{
    vec4 finalColor;
    
    if (useTexture) {
        // Sample texture
        vec4 texColor = texture(theTexture, TexCoord);
        
        // If texture has transparency, discard very transparent pixels
        if (texColor.a < 0.1f) {
            discard;
        }
        
        finalColor = vec4(texColor.rgb, 1.0f);
    } else {
        // Use vertex color if available, otherwise use a neutral gray
        // Check if vertex color is valid (not black/zero which often means unset)
        if (length(vCol.rgb) > 0.01f) {
            finalColor = vec4(vCol.rgb, 1.0f);
        } else {
            // Default to neutral gray for objects without color
            finalColor = vec4(0.7f, 0.7f, 0.7f, 1.0f);
        }
    }
    
    // Output the color directly (unlit)
    colour = finalColor;
}

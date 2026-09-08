//taken from the depth rendering example from raylib
//some modifications make it work on macOS (in instead of varying, out vec4 FragColor instead of default gl_FragColor, texture instead of texture2D)

#version 330 //important to enable the aforementioned language features

precision mediump float;

// pixel coordinate
in vec2 fragTexCoord;

// Input uniform (global) values (supplied via `SetShaderValue` and `SetShaderValueTexture` in main.cpp)
uniform sampler2D depthTexture;
uniform sampler2D colorTexture;
uniform bool flipY;

float nearPlane = 0.1;
float farPlane = 10000.0;

out vec4 FragColor; //instead of gl_FragColor (same purpose)

void main() {
    // Handle potential Y-flipping
    vec2 texCoord = fragTexCoord;
    if (flipY) texCoord.y = 1.0 - texCoord.y;

    // Sample depth texture
    float depth = texture(depthTexture, texCoord).r;
    // Sample color texture 
    vec4 color = texture(colorTexture, texCoord);

    // Linearize depth
    float linearDepth = (2.0*nearPlane)/(farPlane + nearPlane - depth*(farPlane - nearPlane));
    
    // Output final color (original from raylib example code)
    //gl_FragColor = vec4(vec3(linearDepth), 1.0);
    
    //float colorMultiplier = max(linearDepth*-0.15+1.0, 0.5);
    //gl_FragColor = vec4(color.r*colorMultiplier, color.g*colorMultiplier, color.b*colorMultiplier, max(1.0-linearDepth/10.0, 0.3)); //fog effect :D
    
    //max(linearDepth*-0.15+1.0, 0.5);
    if (linearDepth >= 1.0) { //no polygons in this direction so we set alpha=0 and later render the skybox instead
        FragColor = vec4(0.0, 0.0, 0.0, 0.0);
    } else {
        //farther pixels are darker
        float colorMultiplier = sqrt(linearDepth)*-0.6+1.0;//sqrt(linearDepth)*.5+.6;
        colorMultiplier = clamp(colorMultiplier, 0.4, 1.0);
        FragColor = vec4(color.rgb*colorMultiplier, 1.0);
    }
    
}
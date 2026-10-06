#version 330

precision mediump float;

in vec3 fragPosition;
in vec2 fragTexCoord;
in vec4 fragColor;
in vec3 fragNormal;

uniform vec4 colDiffuse;

out vec4 glFragColor;

uniform sampler2D texture0;

float nearPlane = 0.1;
float farPlane = 10000.0;

void main() {
    float depth = (2.0*nearPlane)/(farPlane + nearPlane - gl_FragCoord.z*(farPlane - nearPlane)); //linearize z depth
    glFragColor = vec4(
        vec3(colDiffuse*fragColor) //initial fragment color
        * (dot(normalize(fragNormal), normalize(vec3(0.0, 1.0, 0.0)))*1.0) //lighting
        * (1.0-depth/1.0), //depth shading
    1.0);
}

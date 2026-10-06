#version 100

precision mediump float;

varying vec3 fragPosition;
varying vec2 fragTexCoord;
varying vec4 fragColor;
varying vec3 fragNormal;

uniform vec4 colDiffuse;

uniform sampler2D texture0;

float nearPlane = 0.1;
float farPlane = 10000.0;

void main() {
    float depth = (2.0*nearPlane)/(farPlane + nearPlane - gl_FragCoord.z*(farPlane - nearPlane)); //linearize z depth
    gl_FragColor = vec4(
        vec3(colDiffuse*fragColor) //initial fragment color
        * (dot(normalize(fragNormal), normalize(vec3(0.0, 1.0, 0.0)))*1.0) //lighting
        * (1.0-depth/1.0), //depth shading
    1.0);
}

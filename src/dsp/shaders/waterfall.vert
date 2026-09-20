#version 440

layout(location = 0) in vec2 position;
layout(location = 1) in vec2 texCoord;

layout(location = 0) out vec2 fragTexCoord;

layout(std140, binding = 0) uniform buf {
    float scrollOffset;   // Row scroll offset for circular buffer
    float binCount;       // Actual bin count (passed to fragment shader)
    float textureWidth;   // Texture width (passed to fragment shader)
    float padding;
    vec4 view; // x=start, y=width within source bins; z/w reserved
};

void main() {
    gl_Position = vec4(position, 0.0, 1.0);
    fragTexCoord = vec2(texCoord.x, texCoord.y + scrollOffset);
}

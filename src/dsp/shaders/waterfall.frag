#version 440

layout(location = 0) in vec2 fragTexCoord;
layout(location = 0) out vec4 outColor;

layout(binding = 1) uniform sampler2D waterfallTex;
layout(binding = 2) uniform sampler2D colorLutTex;

layout(std140, binding = 0) uniform buf {
    float scrollOffset;   // Row scroll offset (used by vertex shader)
    float binCount;       // Actual spectrum bin count (e.g., 213 at 5kHz span)
    float textureWidth;   // Texture width for bin centering (1024)
    float padding;
    vec4 view; // x=start, y=width within source bins; z/w reserved
};

void main() {
    // Map display X [0,1] to bin index, then to texture coordinate
    // This gives crisp, non-interpolated waterfall showing actual bin data
    float sourceX = view.x + fragTexCoord.x * view.y;
    if (sourceX < 0.0 || sourceX >= 1.0) { outColor = vec4(0, 0, 0, 1); return; }
    float binIndex = sourceX * binCount;
    float texelSize = 1.0 / textureWidth;

    // Bins are centered in texture - use floor() to match C++ integer division
    float binOffset = floor((textureWidth - binCount) / 2.0);

    // Nearest-neighbor: truncate to get bin, add 0.5 to sample center of texel
    float texU = (binOffset + floor(binIndex) + 0.5) * texelSize;

    float dbValue = texture(waterfallTex, vec2(texU, fragTexCoord.y)).r;
    outColor = texture(colorLutTex, vec2(dbValue, 0.5));
}

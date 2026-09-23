#version 450
layout(location = 0) in vec4 vertexColor;
layout(location = 1) in vec2 texCoord;
layout(location = 0) out vec4 outputColor;
layout(set = 0, binding = 0) uniform sampler2D image;
void main() {
    outputColor = vertexColor * texture(image, texCoord);
}

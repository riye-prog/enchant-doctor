#version 450
layout(location = 0) in vec2 position;
layout(location = 1) in vec4 color;
layout(location = 2) in vec2 uv;
layout(location = 0) out vec4 vertexColor;
layout(location = 1) out vec2 texCoord;
layout(push_constant) uniform Push { mat4 transform; vec2 translation; vec2 extent; } push;
void main() {
    vec4 point = push.transform * vec4(position + push.translation, 0.0, 1.0);
    gl_Position = vec4(2.0 * point.xy / push.extent - point.w, 0.0, point.w);
    vertexColor = color;
    texCoord = uv;
}

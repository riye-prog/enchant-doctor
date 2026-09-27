#version 450
layout(location = 0) in vec4 vertexColor;
layout(location = 1) in vec2 texCoord;
layout(location = 2) in vec2 localPosition;
layout(location = 0) out vec4 outputColor;
layout(set = 0, binding = 0) uniform sampler2D image;
layout(set = 1, binding = 0) uniform sampler2D auxiliary;
layout(std430, set = 2, binding = 0) readonly buffer Effect {
    mat4 matrix;
    vec4 data0;
    vec4 data1;
    vec4 stops[];
} effect;
layout(push_constant) uniform Push {
    mat4 transform;
    vec2 translation;
    vec2 extent;
    vec4 data;
    vec4 color;
    ivec4 config;
} push;
vec4 sampleImage(vec2 uv) {
    if (any(lessThan(uv, vec2(0))) || any(greaterThan(uv, vec2(1)))) return vec4(0);
    return texture(image, uv);
}
vec4 gradient(float t) {
    int count = push.config.y;
    if (count == 0) return vec4(0);
    float first = effect.stops[0].x;
    float last = effect.stops[(count - 1) * 2].x;
    if (push.config.z != 0) t = last > first ? mod(t - first, last - first) + first : last;
    vec4 result = effect.stops[1];
    for (int i = 1; i < count; ++i) {
        float a = effect.stops[(i - 1) * 2].x;
        float b = effect.stops[i * 2].x;
        float factor = b > a ? clamp((t - a) / (b - a), 0.0, 1.0) : step(b, t);
        result = mix(result, effect.stops[i * 2 + 1], factor);
    }
    return result;
}
void main() {
    int mode = push.config.x;
    vec2 uv = texCoord;
    vec4 result;
    if (mode == 2) {
        vec2 stepSize = push.data.xy / vec2(textureSize(image, 0));
        float sigma = max(push.data.z, 0.001);
        float spacing = max(1.0, sigma * 3.0 / 16.0);
        result = vec4(0);
        float weightSum = 0.0;
        for (int i = -16; i <= 16; ++i) {
            float distance = float(i) * spacing;
            float weight = exp(-0.5 * distance * distance / (sigma * sigma));
            result += sampleImage(uv + stepSize * distance) * weight;
            weightSum += weight;
        }
        result /= weightSum;
    } else if (mode == 3) {
        result = effect.matrix * texture(image, uv);
        result = clamp(result, vec4(0), vec4(result.a));
    } else if (mode == 4) {
        result = sampleImage(uv - push.data.xy / vec2(textureSize(image, 0))).a * push.color;
    } else if (mode == 5) {
        result = texture(image, uv) * texture(auxiliary, uv).a;
    } else if (mode == 6) {
        vec4 foreground = texture(auxiliary, uv);
        result = foreground + texture(image, uv) * (1.0 - foreground.a);
    } else if (mode >= 10 && mode <= 12) {
        vec2 p = effect.data0.xy;
        vec2 v = effect.data0.zw;
        vec2 relative = localPosition - p;
        float t = 0.0;
        if (mode == 10) t = dot(relative, v) / max(dot(v, v), 0.000001);
        if (mode == 11) t = length(relative * v);
        if (mode == 12) {
            vec2 rotated = vec2(dot(v, relative), v.x * relative.y - v.y * relative.x);
            t = fract(atan(rotated.x, -rotated.y) / 6.28318530718);
        }
        result = gradient(t) * vertexColor;
    } else {
        result = vertexColor * texture(image, uv);
        if (mode == 1) result *= push.data.x;
    }
    if (push.config.w != 0 && result.a > 0.0) {
        vec3 srgb = result.rgb / result.a;
        result.rgb = mix(srgb / 12.92, pow((srgb + 0.055) / 1.055, vec3(2.4)), step(vec3(0.04045), srgb)) * result.a;
    }
    outputColor = result;
}

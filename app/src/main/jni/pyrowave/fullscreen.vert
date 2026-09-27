#version 450
// Rotate clip space into the display's natural orientation; retain source texture coordinates.
layout(location=0) out vec2 uv;
layout(push_constant) uniform Presentation { vec4 rotation; int hdr; } p;
void main() {
    uv = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    vec2 position = uv * 2.0 - 1.0;
    gl_Position = vec4(dot(p.rotation.xy, position), dot(p.rotation.zw, position), 0.0, 1.0);
}

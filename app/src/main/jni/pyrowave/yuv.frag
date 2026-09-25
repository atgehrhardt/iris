#version 450
layout(location=0) in vec2 uv;
layout(location=0) out vec4 color;
layout(binding=0) uniform sampler2D luma;
layout(binding=1) uniform sampler2D cb;
layout(binding=2) uniform sampler2D cr;
layout(push_constant) uniform Presentation { vec4 rotation; int hdr; } mode;
void main() {
    float y = texture(luma, uv).r;
    float u = texture(cb, uv).r - 0.5;
    float v = texture(cr, uv).r - 0.5;
    // Full-range, centered chroma, BT.709 SDR or BT.2020 NCL PQ.
    vec3 rgb = mode.hdr != 0 ? vec3(y + 1.4746*v, y - 0.164553*u - 0.571353*v, y + 1.8814*u)
                             : vec3(y + 1.5748*v, y - 0.187324*u - 0.468124*v, y + 1.8556*u);
    color = vec4(clamp(rgb, 0.0, 1.0), 1.0);
}

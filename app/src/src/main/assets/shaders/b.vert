#version 450

// 全屏三角形顶点着色器
layout(location = 0) out vec2 fragCoord;

// Push Constants
layout(push_constant) uniform PushConstants {
    vec2 iResolution;
    float iTime;
    float _padding;
} pc;

void main() {
    // 生成全屏三角形的顶点
    vec2 positions[3] = vec2[](
        vec2(-1.0, -1.0),
        vec2( 3.0, -1.0),
        vec2(-1.0,  3.0)
    );

    vec2 pos = positions[gl_VertexIndex];
    gl_Position = vec4(pos, 0.0, 1.0);

    // 将 NDC 坐标 [-1, 1] 转换为屏幕坐标 [0, resolution]
    fragCoord = (pos * 0.5 + 0.5) * pc.iResolution;
}

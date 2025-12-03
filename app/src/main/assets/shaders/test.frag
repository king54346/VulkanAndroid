#version 450

// 从顶点着色器接收 UV
layout(location = 0) in vec2 vUV;

// 使用 Push Constants 替代 Uniform Buffer（更简单）
layout(push_constant) uniform PushConstants {
    vec2 resolution;
    float time;
    float padding;  // 对齐到16字节
} pc;

// 输出
layout(location = 0) out vec4 outColor;

void main() {
    vec2 uv = vUV;

    // 测试1: UV可视化
    vec3 color = vec3(uv.x, uv.y, 0.0);

    // 测试2: 时间动画
    color.r += 0.3 * sin(pc.time);
    color.g += 0.3 * cos(pc.time * 0.7);

    // 测试3: 分辨率检查 - 绿色边框
    vec2 pixelCoord = uv * pc.resolution;
    float borderWidth = 10.0;

    if (pixelCoord.x < borderWidth ||
        pixelCoord.y < borderWidth ||
        pixelCoord.x > pc.resolution.x - borderWidth ||
        pixelCoord.y > pc.resolution.y - borderWidth) {
        color = vec3(0.0, 1.0, 0.0);  // 绿色边框
    }

    // 测试4: 中心十字
    if (abs(pixelCoord.x - pc.resolution.x * 0.5) < 2.0 ||
        abs(pixelCoord.y - pc.resolution.y * 0.5) < 2.0) {
        color = vec3(1.0, 1.0, 1.0);  // 白色十字
    }

    outColor = vec4(color, 1.0);
}

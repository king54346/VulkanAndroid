#version 450

// 输出 UV 给片元着色器
layout(location = 0) out vec2 vUV;

void main() {
    // Vulkan 使用 gl_VertexIndex
    int idx = gl_VertexIndex;

    // 生成全屏三角形坐标
    // 顶点0: (-1, -1), 顶点1: (3, -1), 顶点2: (-1, 3)
    float x = float((idx << 1) & 2);
    float y = float(idx & 2);

    vec2 pos = vec2(x, y);

    // 传递 UV 坐标给片元着色器 (0,0) 到 (1,1)
    vUV = pos * 0.5 + 0.5;

    // 映射到裁剪空间 (-1.0 ~ 1.0)
    gl_Position = vec4(pos * 2.0 - 1.0, 0.0, 1.0);
}

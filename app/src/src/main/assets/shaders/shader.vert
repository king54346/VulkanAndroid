#version 450

layout(location = 0) out vec2 fragTexCoord;

void main() {
    vec2 pos;
    vec2 uv;

    // 修正后的顶点顺序：覆盖整个屏幕
    switch (gl_VertexIndex) {
        case 0: // 左下
            pos = vec2(-1.0, -1.0);
            uv = vec2(0.0, 0.0);
            break;
        case 1: // 右下
            pos = vec2(1.0, -1.0);
            uv = vec2(1.0, 0.0);
            break;
        case 2: // 右上
            pos = vec2(1.0, 1.0);
            uv = vec2(1.0, 1.0);
            break;
        case 3: // 左下（第二个三角形）
            pos = vec2(-1.0, -1.0);
            uv = vec2(0.0, 0.0);
            break;
        case 4: // 右上（第二个三角形）
            pos = vec2(1.0, 1.0);
            uv = vec2(1.0, 1.0);
            break;
        case 5: // 左上
            pos = vec2(-1.0, 1.0);
            uv = vec2(0.0, 1.0);
            break;
    }

    gl_Position = vec4(pos, 0.0, 1.0);
    fragTexCoord = uv;
}
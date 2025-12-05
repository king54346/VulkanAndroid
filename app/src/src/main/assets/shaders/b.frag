#version 450

// ========== Vulkan 输入/输出 ==========
layout(location = 0) in vec2 fragCoord;  // 从 vertex shader 传入 (0-1)
layout(location = 0) out vec4 fragColor;

// ========== Push Constants ==========
layout(push_constant) uniform PushConstants {
    vec2 iResolution;
    float iTime;
    float _padding;
} pc;

// ========== 配置选项 ==========
// 取消注释以显示平铺效果
// #define SHOW_TILING

// ========== 常量定义 ==========
#define TAU 6.28318530718
#define MAX_ITER 5

// ========== 主函数 ==========
void main()
{
    float time = pc.iTime * 0.5 + 23.0;

    // 🔥 fragCoord 是像素坐标，需要除以分辨率得到 0-1 的 UV
    vec2 uv = fragCoord / pc.iResolution.xy;

#ifdef SHOW_TILING
    vec2 p = mod(uv * TAU * 2.0, TAU) - 250.0;
#else
    vec2 p = mod(uv * TAU, TAU) - 250.0;
#endif

    vec2 i = vec2(p);
    float c = 1.0;
    float inten = 0.005;

    // 湍流迭代
    for (int n = 0; n < MAX_ITER; n++)
    {
        float t = time * (1.0 - (3.5 / float(n + 1)));
        i = p + vec2(
            cos(t - i.x) + sin(t + i.y),
            sin(t - i.y) + cos(t + i.x)
        );
        c += 1.0 / length(vec2(
            p.x / (sin(i.x + t) / inten),
            p.y / (cos(i.y + t) / inten)
        ));
    }

    c /= float(MAX_ITER);
    c = 1.17 - pow(c, 1.4);
    vec3 colour = vec3(pow(abs(c), 8.0));
    colour = clamp(colour + vec3(0.0, 0.35, 0.5), 0.0, 1.0);

#ifdef SHOW_TILING
    // 闪烁瓷砖边框
    vec2 pixel = 2.0 / pc.iResolution.xy;
    uv *= 2.0;
    float f = floor(mod(pc.iTime * 0.5, 2.0));     // 闪烁值
    vec2 first = step(pixel, uv) * f;              // 排除首屏像素并闪烁
    uv = step(fract(uv), pixel);                   // 每个瓷砖添加一行像素
    colour = mix(
        colour,
        vec3(1.0, 1.0, 0.0),
        (uv.x + uv.y) * first.x * first.y
    ); // 黄色线条
#endif

    fragColor = vec4(colour, 1.0);
}

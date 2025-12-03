#version 450

layout(location = 0) in vec2 fragCoord;
layout(location = 0) out vec4 outColor;

// Push Constants
layout(push_constant) uniform PushConstants {
    vec2 iResolution;
    float iTime;
    float _padding;
} pc;

/*
 * "Low Tech Tunnel" - 28 steps
 * Optimized Shadertoy shader converted to Vulkan
 */

#define V vec3
#define T_VALUE (pc.iTime * 4.0 + 5.0 + 5.0 * sin(pc.iTime * 0.3))
#define P(z) V(12.0 * cos((z) * vec2(0.1, 0.12)), z)
#define A(F, H, K) abs(dot(sin(F * p * K), H + p - p)) / K

void main() {
    float t, s, i, d, e;
    vec3 c, r;
    vec2 u;

    r = vec3(pc.iResolution, 0.0);
    u = fragCoord;

    // scaled coords
    u = (u - r.xy / 2.0) / r.y;

    // cinema bars
    if (abs(u.y) < 0.375) {
        float T = T_VALUE;

        // setup ray origin, direction, and look-at
        vec3 p = P(T);
        vec3 Z = normalize(P(T + 4.0) - p);
        vec3 X = normalize(vec3(Z.z, 0.0, -Z.x));
        vec3 D = vec3(u, 1.0) * mat3(-X, cross(X, Z), Z);

        i = 0.0;
        d = 0.0;
        c = vec3(0.0);

        // raymarching loop
        for (int iter = 0; iter < 28; iter++) {
            if (i >= 28.0 || d >= 30.0) break;

            // march
            p += D * s;

            // get path
            X = P(p.z);

            // store sine of iTime (not T)
            t = sin(pc.iTime);

            // orb (sphere with xyz offset by t)
            e = length(p - vec3(
                X.x,
                X.y + t,
                6.0 + T + t) - t) - 0.01;

            // tunnel with modulating radius
            s = cos(p.z * 0.6) * 2.0 + 4.0
                - min(length(p.xy - X.x - 6.0),
                      length((p - X).xy))
                + A(4.0, 0.25, 0.1)        // noise, large scoops
                + A(T + 8.0, 0.22, 2.0);   // noise, detail texture

            // accumulate distance
            s = min(e, 0.01 + 0.3 * abs(s));
            d += s;

            // accumulate color
            c += 1.0 / s + vec3(10.0, 20.0, 50.0) / max(e, 0.6);

            i += 1.0;
        }

        // adjust brightness and saturation
        outColor.rgb = c * c / 1e6;
    } else {
        // cinema bars (black)
        outColor.rgb = vec3(0.0);
    }

    outColor.a = 1.0;
}

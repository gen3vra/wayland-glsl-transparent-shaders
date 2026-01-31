#version 460 core

layout(location = 0) out vec4 fragColor;

uniform vec2 u_resolution;
uniform float u_time;

void main() {
    vec2 u = gl_FragCoord.xy;
    vec4 o = vec4(0.0);
    vec3 p = vec3(u_resolution.x, u_resolution.y, 1.0);
    u = (u - u_resolution.xy * 0.5) / u_resolution.y;    
    float d = 0.0;
    float s = 0.0;
    for(int i = 0; i < 100; i++) {
        s = 0.001 + abs(s) * 0.8;
        d += s;
        // Color accumulation
        o += 1.0 / s
           + 0.5 * vec4(10.0, 2.0, 1.0, 0.0) / length(u * sin(u_time * 0.3))
           + 20.0 * vec4(1.0, 2.0, 10.0, 0.0) * dot(u, u);
        // Space transformation
        p = vec3(u * d, d + u_time); 
        // Rotation logic
        float angle = p.z * 0.1;
        float c = cos(angle);
        float st = sin(angle);
        p.xy *= mat2(c, -st, st, c);
        s = cos(p.y + cos(p.x) * 2.0);
        // Noise loop
        for (float n = 0.01; n < 0.4; n += n) {
            s -= abs(dot(cos(0.3 * u_time + 0.7 * p / n), vec3(n)));
        }
    }
    fragColor = tanh(o / 20000.0);
}
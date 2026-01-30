// Rename me to shader.frag
#version 460 core
uniform vec2  u_resolution;
uniform float u_time;
uniform sampler2D iChannel0;

out vec4 fragColor;

void main()
{
    vec2 uv = gl_FragCoord.xy / u_resolution.xy;
    
    // Read previous frame
    vec4 prev = texture(iChannel0, uv);
    prev *= 0.88; // Fade trail
    
    // Pink circle moving in circle on LEFT side
    vec2 center = vec2(0.25 + 0.15 * sin(u_time), 0.5 + 0.15 * cos(u_time));
    float dist = distance(uv, center);
    float circle = 1.0 - smoothstep(0.04, 0.05, dist);
    
    // Combine
    vec3 color = prev.rgb + vec3(1.0, 0.4, 0.8) * circle; // Pink
    
    // Transparency
    float brightness = length(color);
    if (brightness > 0.01) {
        fragColor = vec4(color, min(brightness, 1.0));
    } else {
        fragColor = vec4(0.0);
    }
}
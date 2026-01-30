// Rename me shader1.frag
#version 460 core
uniform vec2  u_resolution;
uniform float u_time;
//uniform sampler2D iChannel0; shader0 tex
uniform sampler2D iChannel1; // this shader tex

out vec4 fragColor;

void main()
{
    vec2 uv = gl_FragCoord.xy / u_resolution.xy;
    // Only render on right half
    if (uv.x < 0.5) {
        fragColor = vec4(0.0);
        return;
    }
    // Read previous frame
    vec4 prev = texture(iChannel1, uv);
    prev *= 0.95; // Fade trail
    
    // Blue circle moving in figure-8 on RIGHT side
    vec2 center = vec2(0.75 + 0.15 * sin(u_time * 1.5), 0.5 + 0.15 * sin(u_time * 3.0));
    float dist = distance(uv, center);
    float circle = 1.0 - smoothstep(0.04, 0.05, dist);
    
    // Combine
    vec3 color = prev.rgb + vec3(0.3, 0.6, 1.0) * circle; // Blue
    
    // Transparency
    float brightness = length(color);
    if (brightness > 0.01) {
        fragColor = vec4(color, min(brightness, 1.0));
    } else {
        fragColor = vec4(0.0);
    }
}

/* Non trailing variant
#version 460 core
uniform vec2  u_resolution;
uniform float u_time;

out vec4 fragColor;

void main()
{
    vec2 uv = gl_FragCoord.xy / u_resolution.xy;

    // Draw a moving circle
    vec2 center = vec2(0.5 + 0.3 * sin(u_time), 0.5 + 0.3 * cos(u_time));
    float dist = distance(uv, center);
    float circle = 1.0 - smoothstep(0.05, 0.06, dist);
    
    vec3 color = vec3(1.0, 0.5, 0.2) * circle;
    
    // TRANSPARENCY: if nothing is there, make it transparent
    float brightness = length(color);
    if (brightness > 0.01) {
        fragColor = vec4(color, min(brightness, 1.0));
    } else {
        fragColor = vec4(0.0);
    }
}*/
#version 460 core
uniform vec2  u_resolution;
uniform float u_time;
uniform sampler2D u_sampler0;

out vec4 fragColor;

void main()
{
    vec2 uv = gl_FragCoord.xy / u_resolution.xy;
    
    // Read previous frame
    vec4 prev = texture(u_sampler0, uv);
    
    // Fade previous frame
    prev *= 0.95;
    
    // Draw a moving circle
    vec2 center = vec2(0.5 + 0.3 * sin(u_time), 0.5 + 0.3 * cos(u_time));
    float dist = distance(uv, center);
    float circle = 1.0 - smoothstep(0.05, 0.06, dist);
    
    // Combine: previous frame faded + new circle
    vec3 color = prev.rgb + vec3(1.0, 0.5, 0.2) * circle;
    
    float brightness = length(color);
    if (brightness > 0.01) {
        fragColor = vec4(color, min(brightness, 1.0));
    } else {
        fragColor = vec4(0.0);
    }
}
#version 460 core
uniform vec2  u_resolution;
uniform float u_time;

out vec4 fragColor;

#define M_PI 3.1415926535897932384626433832795

float rand(vec2 co)
{
    return fract(sin(dot(co.xy, vec2(12.9898, 78.233))) * 43758.5453);
}

void main()
{
    vec2 fragCoord = gl_FragCoord.xy;
    
    float size = 30.0;
    float prob = 0.95;
    
    vec2 pos = floor(1.0 / size * fragCoord.xy);
    
    float color = 0.0;
    float starValue = rand(pos);
    
    if (starValue > prob)
    {
        vec2 center = size * pos + vec2(size, size) * 0.5;
        
        float t = 0.9 + 0.2 * sin(u_time + (starValue - prob) / (1.0 - prob) * 45.0);
                
        color = 1.0 - distance(fragCoord.xy, center) / (0.5 * size);
        color = color * t / (abs(fragCoord.y - center.y)) * t / (abs(fragCoord.x - center.x));
    }
    else if (rand(fragCoord.xy / u_resolution.xy) > 0.996)
    {
        float r = rand(fragCoord.xy);
        color = r * (0.25 * sin(u_time * (r * 5.0) + 720.0 * r) + 0.75);
    }
    
    // Make transparent background instead of black
    if (color > 0.01) {
        fragColor = vec4(vec3(color), color);
    } else {
        fragColor = vec4(0.0);
    }
}
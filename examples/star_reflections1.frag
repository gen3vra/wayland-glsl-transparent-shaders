#version 460 core

layout(location = 0) out vec4 fragColor;

uniform sampler2D u_sampler0;
uniform float u_time; // animated ripple

void main()
{
    ivec2 size  = textureSize(u_sampler0, 0);
    ivec2 coord = ivec2(gl_FragCoord.xy);

    // Mirror vertically (water reflection)
    coord.y = size.y - 1 - coord.y;

    // subtle horizontal wave
    float waveAmplitude = 1.2; // slightly smaller than before
    float waveFrequency = 0.06;
    float xOffset = waveAmplitude * sin(coord.y * waveFrequency + u_time + sin(coord.y * 0.01));
    coord.x = clamp(int(float(coord.x) + xOffset), 0, size.x - 1);

    // optional vertical wobble (very subtle)
    float yOffset = 0.5 * sin(coord.x * 0.02 + u_time * 1.2);
    coord.y = clamp(coord.y + int(yOffset), 0, size.y - 1);

    vec4 data = texelFetch(u_sampler0, coord, 0);

    // lower alpha for watery effect
    data.a *= 0.3;

    fragColor = data;
}

#version 460 core

uniform vec2  u_resolution;
uniform sampler2D u_sampler0;
out vec4 fragColor;

void main()
{
	vec2 uv = gl_FragCoord.xy / u_resolution.xy;
	fragColor = texture(u_sampler0,uv);
}
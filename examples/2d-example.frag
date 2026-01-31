#version 460 core

layout(location = 0) out vec4 fragColor;

uniform float u_time;
uniform vec2  u_resolution;
uniform vec2  u_mouse;
uniform sampler2D u_sampler0;

//////////////////////////////////////
// Combine distance field functions //
//////////////////////////////////////

float smoothMerge(float d1, float d2, float k)
{
    float h = clamp(0.5 + 0.5 * (d2 - d1) / k, 0.0, 1.0);
    return mix(d2, d1, h) - k * h * (1.0 - h);
}

float merge(float d1, float d2)
{
    return min(d1, d2);
}

float mergeExclude(float d1, float d2)
{
    return min(max(-d1, d2), max(-d2, d1));
}

float substract(float d1, float d2)
{
    return max(-d1, d2);
}

float intersect(float d1, float d2)
{
    return max(d1, d2);
}

//////////////////////////////
// Rotation and translation //
//////////////////////////////

vec2 rotateCCW(vec2 p, float a)
{
    mat2 m = mat2(cos(a), sin(a), -sin(a), cos(a));
    return p * m;
}

vec2 rotateCW(vec2 p, float a)
{
    mat2 m = mat2(cos(a), -sin(a), sin(a), cos(a));
    return p * m;
}

vec2 translate(vec2 p, vec2 t)
{
    return p - t;
}

//////////////////////////////
// Distance field functions //
//////////////////////////////

float pie(vec2 p, float angle)
{
    angle = radians(angle) / 2.0;
    vec2 n = vec2(cos(angle), sin(angle));
    return abs(p).x * n.x + p.y * n.y;
}

float circleDist(vec2 p, float radius)
{
    return length(p) - radius;
}

float triangleDist(vec2 p, float radius)
{
    return max(abs(p).x * 0.866025 + p.y * 0.5, -p.y) - radius * 0.5;
}

float triangleDist(vec2 p, float width, float height)
{
    vec2 n = normalize(vec2(height, width / 2.0));
    return max(abs(p).x * n.x + p.y * n.y - (height * n.y), -p.y);
}

float semiCircleDist(vec2 p, float radius, float angle, float width)
{
    width /= 2.0;
    radius -= width;
    return substract(
        pie(p, angle),
        abs(circleDist(p, radius)) - width
    );
}

float boxDist(vec2 p, vec2 size, float radius)
{
    size -= vec2(radius);
    vec2 d = abs(p) - size;
    return min(max(d.x, d.y), 0.0) + length(max(d, 0.0)) - radius;
}

float lineDist(vec2 p, vec2 start, vec2 end, float width)
{
    vec2 dir = start - end;
    float lngth = length(dir);
    dir /= lngth;
    vec2 proj = max(0.0, min(lngth, dot((start - p), dir))) * dir;
    return length((start - p) - proj) - (width / 2.0);
}

///////////////////////
// Masks for drawing //
///////////////////////

float fillMask(float dist)
{
    return clamp(-dist, 0.0, 1.0);
}

float innerBorderMask(float dist, float width)
{
    float a1 = clamp(dist + width, 0.0, 1.0);
    float a2 = clamp(dist, 0.0, 1.0);
    return a1 - a2;
}

float outerBorderMask(float dist, float width)
{
    float a1 = clamp(dist, 0.0, 1.0);
    float a2 = clamp(dist - width, 0.0, 1.0);
    return a1 - a2;
}

///////////////
// The scene //
///////////////

float sceneDist(vec2 p)
{
    float c  = circleDist(translate(p, vec2(100.0, 250.0)), 40.0);
    float b1 = boxDist(translate(p, vec2(200.0, 250.0)), vec2(40.0), 0.0);
    float b2 = boxDist(translate(p, vec2(300.0, 250.0)), vec2(40.0), 10.0);
    float l  = lineDist(p, vec2(370.0, 220.0), vec2(430.0, 280.0), 10.0);
    float t1 = triangleDist(translate(p, vec2(500.0, 210.0)), 80.0, 80.0);
    float t2 = triangleDist(rotateCW(translate(p, vec2(600.0, 250.0)), u_time), 40.0);

    float m = merge(c, b1);
    m = merge(m, b2);
    m = merge(m, l);
    m = merge(m, t1);
    m = merge(m, t2);

    float b3 = boxDist(
        translate(p, vec2(100.0, sin(u_time * 3.0 + 1.0) * 40.0 + 100.0)),
        vec2(40.0, 15.0), 0.0
    );
    float c2 = circleDist(translate(p, vec2(100.0, 100.0)), 30.0);
    float s = substract(b3, c2);

    float b4 = boxDist(
        translate(p, vec2(200.0, sin(u_time * 3.0 + 2.0) * 40.0 + 100.0)),
        vec2(40.0, 15.0), 0.0
    );
    float c3 = circleDist(translate(p, vec2(200.0, 100.0)), 30.0);
    float i = intersect(b4, c3);

    float b5 = boxDist(
        translate(p, vec2(300.0, sin(u_time * 3.0 + 3.0) * 40.0 + 100.0)),
        vec2(40.0, 15.0), 0.0
    );
    float c4 = circleDist(translate(p, vec2(300.0, 100.0)), 30.0);
    float a = merge(b5, c4);

    float b6 = boxDist(translate(p, vec2(400.0, 100.0)), vec2(40.0, 15.0), 0.0);
    float c5 = circleDist(translate(p, vec2(400.0, 100.0)), 30.0);
    float sm = smoothMerge(b6, c5, 10.0);

    float sc = semiCircleDist(translate(p, vec2(500.0, 100.0)), 40.0, 90.0, 10.0);

    float b7 = boxDist(
        translate(p, vec2(600.0, sin(u_time * 3.0 + 3.0) * 40.0 + 100.0)),
        vec2(40.0, 15.0), 0.0
    );
    float c6 = circleDist(translate(p, vec2(600.0, 100.0)), 30.0);
    float e = mergeExclude(b7, c6);

    m = merge(m, s);
    m = merge(m, i);
    m = merge(m, a);
    m = merge(m, sm);
    m = merge(m, sc);
    m = merge(m, e);

    return m;
}

float sceneSmooth(vec2 p, float r)
{
    float a = sceneDist(p);
    a += sceneDist(p + vec2(0.0, r));
    a += sceneDist(p + vec2(0.0, -r));
    a += sceneDist(p + vec2(r, 0.0));
    a += sceneDist(p + vec2(-r, 0.0));
    return a / 5.0;
}

//////////////////////
// Shadow and light //
//////////////////////

float shadow(vec2 p, vec2 pos, float radius)
{
    vec2 dir = normalize(pos - p);
    float dl = length(p - pos);
    float lf = radius * dl;
    float dt = 0.01;

    for (int i = 0; i < 64; ++i)
    {
        float sd = sceneDist(p + dir * dt);
        if (sd < -radius) return 0.0;

        lf = min(lf, sd / dt);
        dt += max(1.0, abs(sd));
        if (dt > dl) break;
    }

    lf = clamp((lf * dl + radius) / (2.0 * radius), 0.0, 1.0);
    return smoothstep(0.0, 1.0, lf);
}

vec4 drawLight(vec2 p, vec2 pos, vec4 color, float dist, float range, float radius)
{
    float ld = length(p - pos);
    if (ld > range) return vec4(0.0);

    float shad = shadow(p, pos, radius);
    float fall = (range - ld) / range;
    fall *= fall;

    float source = fillMask(circleDist(p - pos, radius));
    return (shad * fall + source) * color;
}

float luminance(vec4 col)
{
    return 0.2126 * col.r + 0.7152 * col.g + 0.0722 * col.b;
}

void setLuminance(inout vec4 col, float lum)
{
    col *= lum / luminance(col);
}

float AO(vec2 p, float dist, float radius, float intensity)
{
    float a = clamp(dist / radius, 0.0, 1.0) - 1.0;
    return 1.0 - (pow(abs(a), 5.0) + 1.0) * intensity + (1.0 - intensity);
}

/////////////////
// The program //
/////////////////

void main()
{
    vec2 p = gl_FragCoord.xy + vec2(0.5);
    vec2 c = u_resolution * 0.5;

    float dist = sceneDist(p);

    vec2 light1Pos = u_mouse;
    vec4 light1Col = vec4(0.75, 1.0, 0.5, 1.0);
    setLuminance(light1Col, 0.4);

    vec2 light2Pos = vec2(u_resolution.x * (sin(u_time + 3.1415) + 1.2) / 2.4, 175.0);
    vec4 light2Col = vec4(1.0, 0.75, 0.5, 1.0);
    setLuminance(light2Col, 0.5);

    vec2 light3Pos = vec2(u_resolution.x * (sin(u_time) + 1.2) / 2.4, 340.0);
    vec4 light3Col = vec4(0.5, 0.75, 1.0, 1.0);
    setLuminance(light3Col, 0.6);

    vec4 col = vec4(0.5) * (1.0 - length(c - p) / u_resolution.x);
    col *= clamp(min(mod(p.y, 10.0), mod(p.x, 10.0)), 0.9, 1.0);
    col *= AO(p, sceneSmooth(p, 10.0), 40.0, 0.4);

    col += drawLight(p, light1Pos, light1Col, dist, 150.0, 6.0);
    col += drawLight(p, light2Pos, light2Col, dist, 200.0, 8.0);
    col += drawLight(p, light3Pos, light3Col, dist, 300.0, 12.0);

    col = mix(col, vec4(1.0, 0.4, 0.0, 1.0), fillMask(dist));
    col = mix(col, vec4(0.1, 0.1, 0.1, 1.0), innerBorderMask(dist, 1.5));

    fragColor = clamp(col, 0.0, 1.0);
}

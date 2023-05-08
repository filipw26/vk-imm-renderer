#version 450

layout (location = 0) out vec4 outColor;

void main(){
    outColor = vec4(0.0, 0.0, 1.0, 1.0);
}

/*
vec3 glowColor = vec3(1.0, 0.0, 0.0);

float sdRoundBox( in vec2 p, in vec2 b, in vec4 r )
{
    r.xy = (p.x>0.0)?r.xy : r.zw;
    r.x  = (p.y>0.0)?r.x  : r.y;
    vec2 q = abs(p)-b+r.x;
    return min(max(q.x,q.y),0.0) + length(max(q,0.0)) - r.x;
}

void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
    // Normalized pixel coordinates (from 0 to 1)
    vec2 uv = fragCoord/iResolution.xy;

    float ar = iResolution.x / iResolution.y;

    vec2 center = vec2(500.0, 300.0);

    float d = sdRoundBox(fragCoord-center, vec2(100.0, 100.0), vec4(50.0));

    d = 1.0 - smoothstep(.0, 1.0, d-1.0);

    float g = max(d - 1.0, 0.0);
    g = min(g, 1.0);


    // Time varying pixel color
    //vec3 col = 0.5 + 0.5*cos(iTime+uv.xyx+vec3(0,2,4));

    vec3 col = glowColor * g;

    //col = vec3(0.0, 0.3, 1.0) * d;



    // Output to screen
    fragColor = vec4(col,1.0);
}
*/
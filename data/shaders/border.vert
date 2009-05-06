
uniform float tex_scroll;
uniform float fog_beta;
uniform float fog_offset;

varying float fog_factor;


#include "inc/fog.ivert"

void main()
{
    gl_FrontColor = gl_Color;

    gl_TexCoord[0] = vec4(gl_MultiTexCoord0.x + tex_scroll, gl_MultiTexCoord0.yzw);

    gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
    
    fog_factor = calcFogFactor(gl_Position.z, fog_offset, fog_beta);
}


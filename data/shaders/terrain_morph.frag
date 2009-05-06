


uniform sampler2D colormap;
uniform sampler2D colormap_next_lvl;

uniform sampler2D detail_tex0;
uniform sampler2D detail_tex1;
uniform sampler2D detail_tex2;
uniform sampler2D detail_tex3;
uniform sampler2D detail_tex4;
uniform sampler2D detail_tex5;


uniform sampler2DShadow shadow_texture;

varying vec4 detail_coeff_unpacked[2];

varying vec4 texcoord_lm;
varying vec4 texcoord_detail[3];

#ifdef SHADOW
varying vec4 texcoord_shadow;
#endif


#include "inc/shadow.ifrag"

//------------------------------------------------------------------------------
void main()
{
    vec3 color        = texture2D(colormap,          texcoord_lm.xy).rgb;

#ifndef OUTERMOST_LEVEL    
    vec3 color_parent = texture2D(colormap_next_lvl, texcoord_lm.zw).rgb;
    color = mix(color, color_parent, detail_coeff_unpacked[1].w);
#endif

#ifdef SHADOW
    color *= sampleShadowMap(shadow_texture, texcoord_shadow);
#endif
    
    color += gl_LightSource[0].ambient.rgb;

    
    vec3 detail = texture2D(detail_tex0, texcoord_detail[0].xy).rgb * detail_coeff_unpacked[0][0];
    detail     += texture2D(detail_tex1, texcoord_detail[0].zw).rgb * detail_coeff_unpacked[0][1];
    detail     += texture2D(detail_tex2, texcoord_detail[1].xy).rgb * detail_coeff_unpacked[0][2];
    detail     += texture2D(detail_tex3, texcoord_detail[1].zw).rgb * detail_coeff_unpacked[0][3];
    detail     += texture2D(detail_tex4, texcoord_detail[2].xy).rgb * detail_coeff_unpacked[1][0];
    detail     += texture2D(detail_tex5, texcoord_detail[2].zw).rgb * detail_coeff_unpacked[1][1];
    
#ifdef XXX_DEBUG_0
    detail = vec3(1.0);
#endif

    gl_FragColor.rgb = mix(gl_Fog.color.rgb, color*detail, detail_coeff_unpacked[1].z);

    // Test whether detail coefficient sum is 1
//      gl_FragColor = vec4((dot(vec4(1.0), detail_coeff_unpacked[0]) +
//                           dot(vec2(1.0), detail_coeff_unpacked[1].rg)) == 1.0);
}

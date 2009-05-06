
uniform sampler2D base_texture;

#ifdef SHADOW
uniform sampler2DShadow shadow_texture;
#endif



varying float diffuse;
varying float alpha_threshold;


#include "inc/shadow.ifrag"


//------------------------------------------------------------------------------
void main() 
{   
    gl_FragColor = texture2D( base_texture, gl_TexCoord[0].xy );

    if (gl_FragColor.a < alpha_threshold) discard;

#ifdef XXX_DEBUG_0
    gl_FragColor = vec4(1.0);
#endif
    
    float final_diffuse = diffuse;

#ifdef SHADOW
    final_diffuse *= sampleShadowMap(shadow_texture, gl_TexCoord[2]);
#endif    

    final_diffuse += gl_LightSource[0].ambient.r;
    
    gl_FragColor.rgb = gl_FragColor.rgb*final_diffuse;
}

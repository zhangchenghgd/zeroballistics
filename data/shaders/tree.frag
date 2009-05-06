
uniform sampler2D base_texture;



varying float light;

varying float fog_factor;


//------------------------------------------------------------------------------
void main() 
{   
    gl_FragColor = texture2D( base_texture, gl_TexCoord[0].xy );

    if (gl_FragColor.a < 0.5) discard;

#ifdef XXX_DEBUG_0
    gl_FragColor = vec4(1.0);
#endif

    gl_FragColor.rgb = gl_FragColor.rgb*light;
    gl_FragColor.rgb = mix(gl_Fog.color.rgb, gl_FragColor.rgb, fog_factor);
}

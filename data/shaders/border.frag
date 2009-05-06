
uniform sampler2D base_texture;

varying float fog_factor;

void main() 
{
    vec4 base_tex_val = texture2D( base_texture, gl_TexCoord[0].xy ) * gl_Color;
    gl_FragColor = vec4(mix(gl_Fog.color.rgb, base_tex_val.rgb, fog_factor), base_tex_val.a);
}

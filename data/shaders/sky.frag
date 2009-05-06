

uniform sampler2D base_texture;

//------------------------------------------------------------------------------
void main() 
{
    gl_FragColor = texture2D( base_texture, gl_TexCoord[0].xy );
}

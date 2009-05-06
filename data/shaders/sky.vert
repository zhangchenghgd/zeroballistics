

//------------------------------------------------------------------------------
void main()
{
    gl_TexCoord[0] = gl_MultiTexCoord0;

    // Sky should not be influenced by camera position -> cancel
    // translation by setting w to zero.
    gl_Position = vec4((gl_ModelViewMatrix * vec4(gl_Vertex.xyz, 0.0)).xyz, 1.0);
    gl_Position = gl_ProjectionMatrix * gl_Position;  
}


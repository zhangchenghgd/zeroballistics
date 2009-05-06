
uniform mat4 proj_tex_mat;

uniform float fog_beta;
uniform float fog_offset;
varying float fog_factor;


varying vec4 proj_view_coords;
varying vec2 tex_coords_norm;
varying vec2 tex_coords_dudv;
varying vec3 light_dir_tangentspace;
varying vec3 eye_dir_tangentspace;


#include "inc/fog.ivert"

//------------------------------------------------------------------------------
void main()
{
    const float size_normalmap = 0.19;
    const float size_dudvmap = 0.15;
    
    
	/* Because we have a flat plane for water we already know the vectors for tangent space 
	vec3 tangent = vec3(1.0, 0.0, 0.0);
	vec3 normal = vec3(0.0, 1.0, 0.0);
	vec3 bi_tangent = vec3(0.0, 0.0, 1.0);
        
    our tangent space is actually the identity matrix, so explicit transformation is not needed,
    in this special case.
    */

    
    // Bring vertex into view space. With inverse NormalMatrix (right hand side multiply == inverse) 
    // ... comment..
	eye_dir_tangentspace = vec3(((gl_ModelViewMatrix * gl_Vertex).xyz * gl_NormalMatrix));

    // First transform into object space via gl_NormalMatrix, into TS as described above
	light_dir_tangentspace = gl_LightSource[0].position.xyz * gl_NormalMatrix;

#ifdef REFLECTIONS
    // projective texture coordinates for reflection texture appliance
   	proj_view_coords = proj_tex_mat * gl_Vertex;
#else
    gl_TexCoord[0] = gl_MultiTexCoord0;
#endif 


    // resize normal/dudvmap
    tex_coords_norm = gl_Vertex.xz * size_normalmap;
    tex_coords_dudv = gl_Vertex.xz * size_dudvmap;    
        
    gl_Position = gl_ModelViewProjectionMatrix *  gl_Vertex;

    // fog factor
    fog_factor = calcFogFactor(gl_Position.z, fog_offset, fog_beta);
}


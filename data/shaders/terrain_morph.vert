
const float FRAC_FACTOR = 1.0  / 16.0;
const float D1_FACTOR   = 1.0  / 15.0;
const float D2_FACTOR   = 16.0 / 15.0;


uniform float fog_beta;
uniform float fog_offset;



#ifdef SHADOW
uniform mat4 proj_texture_mat;
varying vec4 texcoord_shadow;
#endif

#ifndef OUTERMOST_LEVEL
uniform vec2 offset_parent;
attribute float height_next_lvl;
#endif


uniform float detail_tex_scale;

// because vec3s use up 4 varying floats, do this workaround...
// detail_coeff_unpacked[1].z is fog factor
// detail_coeff_unpacked[1].w is blend factor
varying vec4 detail_coeff_unpacked[2];


attribute vec4  detail_coeff;
attribute vec4  detail_coeff_next_lvl;


uniform float inv_colormap_extents;
uniform vec3 camera_pos;

uniform float morph_k;
uniform float morph_d;

uniform vec2 offset;


uniform mat3 tex_mat0;
uniform mat3 tex_mat1;
uniform mat3 tex_mat2;
uniform mat3 tex_mat3;
uniform mat3 tex_mat4;
uniform mat3 tex_mat5;


varying vec4 texcoord_lm;
varying vec4 texcoord_detail[3];

#include "inc/fog.ivert"

void main()
{    
    vec2 diff = abs(camera_pos.xz - gl_Vertex.xz);
    detail_coeff_unpacked[1].w = clamp(max(diff.x, diff.y)*morph_k + morph_d, 0.0, 1.0);


    texcoord_lm.xy = (gl_Vertex.xz + offset.xy)*inv_colormap_extents;
    

#ifdef OUTERMOST_LEVEL
    vec4 morphed_position = gl_Vertex;
#else
    float height = mix(gl_Vertex.y, height_next_lvl, detail_coeff_unpacked[1].w);
    vec4 morphed_position = vec4(gl_Vertex.x, height, gl_Vertex.z, 1.0);

    // Texcoord for parent lm, not needed at outermost level
    texcoord_lm.zw = (gl_Vertex.xz + offset_parent.xy)*inv_colormap_extents*0.5;
#endif  
        
    gl_Position = gl_ModelViewProjectionMatrix * morphed_position;


    
#ifdef SHADOW    
    texcoord_shadow = proj_texture_mat * (gl_ModelViewMatrix * gl_Vertex);
#endif



    texcoord_detail[0].xy = (tex_mat0 * morphed_position.xyz).xy;
    texcoord_detail[0].zw = (tex_mat1 * morphed_position.xyz).xy;
    texcoord_detail[1].xy = (tex_mat2 * morphed_position.xyz).xy;
    texcoord_detail[1].zw = (tex_mat3 * morphed_position.xyz).xy;
    texcoord_detail[2].xy = (tex_mat4 * morphed_position.xyz).xy;
    texcoord_detail[2].zw = (tex_mat5 * morphed_position.xyz).xy;



    // fog factor
    detail_coeff_unpacked[1].z = calcFogFactor(gl_Position.z, fog_offset, fog_beta);


    vec4 frac_coeff = detail_coeff * FRAC_FACTOR;
#ifdef OUTERMOST_LEVEL
    detail_coeff_unpacked[0]    = floor(frac_coeff   ) * D1_FACTOR;
    detail_coeff_unpacked[1].xy = fract(frac_coeff.xy) * D2_FACTOR;
#else
    vec4 frac_coeff_next_lvl = detail_coeff_next_lvl * FRAC_FACTOR;
    
    detail_coeff_unpacked[0]    = mix(floor(frac_coeff),
                                      floor(frac_coeff_next_lvl),
                                      detail_coeff_unpacked[1].w) * D1_FACTOR;
    detail_coeff_unpacked[1].xy = mix(fract(frac_coeff.xy),
                                      fract(frac_coeff_next_lvl.xy),
                                      detail_coeff_unpacked[1].w) * D2_FACTOR;
#endif
}


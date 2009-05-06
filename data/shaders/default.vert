
uniform float specularity;
uniform float hardness;
uniform float elapsed_time;

#ifdef SHADOW
uniform mat4 proj_texture_mat;
#endif


uniform float fog_beta;
uniform float fog_offset;
varying float fog_factor;

#ifdef INSTANCED
uniform vec3 instance_data[NUM_VEC3S_PER_INSTANCE*INSTANCE_BATCH_SIZE];
#endif


#ifdef LIGHTING

#ifdef BUMP_MAP
attribute vec3 tangent;
attribute vec3 bi_tangent;

varying vec3 light_dir;
varying vec3 eye_dir;
varying vec3 color;
#else

#ifdef PP_LIGHTING
varying vec3 eye_dir;
varying vec3 normal;
varying vec3 color;
#else

vec3 eye_dir;
vec3 normal;
varying vec3 diffuse;
varying vec3 spec;

#endif // PP_LIGHTING
#endif // BUMP_MAP
#endif // LIGHTING


#include "inc/fog.ivert"

#ifdef INSTANCED
#include "inc/instance.ivert"
#endif

//------------------------------------------------------------------------------
void main()
{
    gl_TexCoord[0] = gl_MultiTexCoord0;


#ifdef INSTANCED
    vec3 pos;
    float angle, scale, inst_diffuse;

    pos       = instance_data[int(gl_MultiTexCoord2.x)*NUM_VEC3S_PER_INSTANCE  ];
    vec3 data = instance_data[int(gl_MultiTexCoord2.x)*NUM_VEC3S_PER_INSTANCE+1];
    angle        = data[0];
    scale        = data[1];
    inst_diffuse = data[2];
    

    vec4 world_pos = applyScaleRotation(gl_Vertex, scale, angle);
    world_pos.xyz += pos;
    
#else
    vec4 world_pos = gl_Vertex;
#endif
    
    
    gl_Position = gl_ModelViewProjectionMatrix * world_pos;

    fog_factor = calcFogFactor(gl_Position.z, fog_offset, fog_beta);

    

// The remaining shader is of interest only if doing some sort of
// lighting.
#ifdef LIGHTING
    
#ifdef BUMP_MAP
    // Transform all varying vectors into tangent space

    // First transform into object space via gl_NormalMatrix
    // (multiplication order makes inverse), then into tangent space
    mat3 m = gl_NormalMatrix * mat3(tangent, bi_tangent, gl_Normal);
    
    eye_dir   = -(gl_ModelViewMatrix * world_pos).xyz * m;
    light_dir = gl_LightSource[0].position.xyz * m;
#else
    
    // Transform all varying vectors into camera space
    normal  =  (gl_NormalMatrix    * gl_Normal).xyz;
    eye_dir = -(gl_ModelViewMatrix * world_pos).xyz;
    
#ifndef PP_LIGHTING
    // No per-pixel lighting, immediately calculate diffuse and
    // specular lighting at the vertex.
    eye_dir = normalize(eye_dir);
    diffuse = vec3(max(dot(gl_LightSource[0].position.xyz, normal),0.0));
    
    spec = vec3(pow(max(dot(eye_dir, reflect(-gl_LightSource[0].position.xyz, normal)),0.0),
                    hardness) * specularity);
    
#endif // PP_LIGHTING
    
#endif // BUMP_MAP



    // Color is used for shading, either apply directly or store for per-pixel lighting
#ifdef INSTANCED
#ifdef PP_LIGHTING
    color = vec3(inst_diffuse);
#else
    diffuse *= inst_diffuse;
    spec    *= inst_diffuse;
#endif        
#else
#ifdef PP_LIGHTING
    color = gl_Color.rgb;
#else
    diffuse *= gl_Color.rgb;
    spec    *= gl_Color.rgb;
#endif // PP_LIGHTING
#endif // INSTANCED
    
    
#ifdef LIGHT_MAP
    gl_TexCoord[1] = gl_MultiTexCoord1;
#endif
#ifdef SHADOW
    gl_TexCoord[2] =  proj_texture_mat * gl_ModelViewMatrix * world_pos;
#endif



#endif // LIGHTING    

    
}


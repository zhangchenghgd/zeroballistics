


uniform float elapsed_time;

varying float light;

uniform float fog_beta;
uniform float fog_offset;
varying float fog_factor;


uniform vec3 instance_data[NUM_VEC3S_PER_INSTANCE*INSTANCE_BATCH_SIZE];


#include "inc/fog.ivert"
#include "inc/instance.ivert"

#ifndef INSTANCED
shader_works_instanced_only
#endif

//------------------------------------------------------------------------------
void main()
{
    vec3 pos;
    float angle, scale;
    pos       = instance_data[int(gl_MultiTexCoord2.x)*NUM_VEC3S_PER_INSTANCE  ];
    vec3 data = instance_data[int(gl_MultiTexCoord2.x)*NUM_VEC3S_PER_INSTANCE+1];
    angle   = data[0];
    scale   = data[1];


    vec4 world_pos = applyScaleRotation(gl_Vertex, scale, angle);
    

    vec3 world_light = normalize(gl_LightSource[0].position.xyz * gl_NormalMatrix);
    vec3 normal;
    normal.y = 0.1;
    normal.xz = world_pos.xz;
    normal = normalize(normal);
    light = clamp(dot(world_light, normal),0.0, 0.7);
    // Empirical values...
    // Lighten further away from trunk and towards the top
    float radial_distance = length(gl_Vertex.xz);    
    light += clamp(radial_distance, 0.0, 0.4);
    light += gl_Vertex.y*0.1;
    light += gl_LightSource[0].ambient.r;

    
    float cosine   = cos(elapsed_time)*0.008;  
//    world_pos.x   += cosine*gl_Vertex.y;
    world_pos.y   += cos(1.4*elapsed_time)*radial_distance*0.008;
//    world_pos.z   += cosine*gl_Vertex.y;

    
    world_pos.xyz += pos;
    
    gl_Position = gl_ModelViewProjectionMatrix * world_pos;


    gl_TexCoord[0] = gl_MultiTexCoord0;
    

    fog_factor = calcFogFactor(gl_Position.z, fog_offset, fog_beta);
}


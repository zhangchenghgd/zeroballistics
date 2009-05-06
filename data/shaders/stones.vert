

uniform float elapsed_time;

uniform mat4 proj_texture_mat;



varying float diffuse;
varying float alpha_threshold;

uniform vec3 camera_pos;
uniform vec3 instance_data[NUM_VEC3S_PER_INSTANCE*INSTANCE_BATCH_SIZE];


#include "inc/instance.ivert"

//------------------------------------------------------------------------------
void main()
{
    vec3 pos;
    float angle, scale;
    pos       = instance_data[int(gl_MultiTexCoord2.x)*NUM_VEC3S_PER_INSTANCE  ];
    vec3 data = instance_data[int(gl_MultiTexCoord2.x)*NUM_VEC3S_PER_INSTANCE+1];
    angle   = data[0];
    scale   = data[1];
    diffuse = data[2];

    
    vec4 world_pos = applyScaleRotation(gl_Vertex, scale, angle);
    world_pos.xyz += pos;


        
    gl_Position = gl_ModelViewProjectionMatrix * world_pos;

    gl_TexCoord[0] = gl_MultiTexCoord0;
    gl_TexCoord[2] = proj_texture_mat * gl_ModelViewMatrix * world_pos;


    float num_layer = floor(angle*0.15915494); // multiple of 2*pi determines multiple of base_draw_dist
    float dist      = BASE_DRAW_DIST*num_layer;
    vec2  diff      = abs(camera_pos.xz - world_pos.xz);
    alpha_threshold = 1.0 - clamp(0.4*(dist - max(diff.x,diff.y)), 0.0, 0.5);


    
    vec3 normal  =  (gl_NormalMatrix    * gl_Normal).xyz;
    vec3 eye_dir = normalize(-(gl_ModelViewMatrix * world_pos).xyz);

    // don't let those look too dark...
    diffuse *= max(dot(gl_LightSource[0].position.xyz, normal), 0.5);
}


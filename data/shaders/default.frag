


uniform sampler2D base_texture;

varying float fog_factor;


#ifdef LIGHTING

uniform float specularity;
uniform float hardness;


#ifdef LIGHT_MAP
uniform sampler2D light_texture;
#endif
#ifdef SHADOW
uniform sampler2DShadow shadow_texture;
#endif
#ifdef EMISSIVE_MAP
uniform sampler2D emissive_texture;
#endif



#ifdef BUMP_MAP
varying vec3 eye_dir;
varying vec3 light_dir;

uniform float inv_bump_map_size;
uniform float parallax_strength;
uniform float normal_bias;
#else

#ifdef PP_LIGHTING
varying vec3 eye_dir;
varying vec3 normal;
#else
varying vec3 diffuse;
varying vec3 spec;
#endif // PP_LIGHTING
#endif // BUMP_MAP

#ifdef PP_LIGHTING
varying vec3 color;
#endif

#endif // LIGHTING



#include "inc/shadow.ifrag"


//------------------------------------------------------------------------------
void main() 
{
#ifdef BUMP_MAP
    vec3 eye_dir_norm = normalize(eye_dir);
    
    float height = texture2D( base_texture, gl_TexCoord[0].xy ).a;
    float height_offset = parallax_strength*(height - 0.5);
    vec2 tex_offset = eye_dir_norm.xy * height_offset;

    vec2 tex_coords = gl_TexCoord[0].xy + tex_offset;
    gl_FragColor = texture2D( base_texture, tex_coords);
#ifdef ALPHA_TEST
    if (gl_FragColor.a < 0.5) discard;
#endif // ALPHA_TEST

    vec3 normal = vec3(gl_FragColor.a - texture2D( base_texture, tex_coords + vec2(inv_bump_map_size, 0.0) ).a,
                       gl_FragColor.a - texture2D( base_texture, tex_coords + vec2(0.0, inv_bump_map_size )).a,
                       normal_bias);

    normal    = normalize(normal);
    vec3 light_dir_norm = normalize(light_dir);
    
    vec3 diffuse, spec;
    
    diffuse = vec3(max(dot(light_dir_norm, normal),0.0));    
    spec    = vec3(pow(max(dot(eye_dir_norm, reflect(-light_dir_norm, normal)), 0.0),
                       hardness) * specularity);

    // attenuate lighting if face is backfacing
    float att_fac = step(0.0, light_dir_norm.z)*0.7+0.3;
    spec    *= att_fac;
    diffuse *= att_fac;
    
#else // BUMP_MAP
    
    gl_FragColor = texture2D( base_texture, gl_TexCoord[0].xy );
#ifdef ALPHA_TEST
    if (gl_FragColor.a < 0.5) discard;
#endif // ALPHA_TEST

    
#ifdef PP_LIGHTING
    vec3 normal_vector  = normalize(normal);
    vec3 eye_dir_norm = normalize(eye_dir);
    
    vec3 diffuse = vec3(max(dot(gl_LightSource[0].position.xyz, normal_vector),0.0));

    vec3 spec = vec3(pow(max(dot(eye_dir_norm, reflect(-gl_LightSource[0].position.xyz, normal_vector)),0.0),
                         hardness) * specularity);

#endif // PP_LIGHTING
    
#endif // BUMP_MAP

#ifdef XXX_DEBUG_0
    gl_FragColor = vec4(1.0);
#endif
//    diffuse = vec3(0.0);
//    spec = vec3(0.0);



#ifdef LIGHTING

    // We can't write to varying variables on ATI cards...
    vec3 final_diffuse = diffuse;
    vec3 final_spec    = spec;

#ifdef EMISSIVE_MAP
    float em = texture2D( emissive_texture, gl_TexCoord[0].xy ).a;    
    final_spec    *= 1.0 - em;
    final_diffuse *= 1.0 - em;
#endif



#ifdef SHADOW
    float shadow_fac = sampleShadowMap(shadow_texture, gl_TexCoord[2]);
#else
    float shadow_fac = 1.0;
#endif // SHADOW

#ifdef PP_LIGHTING
    shadow_fac = min(shadow_fac, color.r);    
#endif // PP_LIGHTING
    

#ifdef EMISSIVE_MAP
    shadow_fac = min(1.0, shadow_fac + em);
#endif
    
    final_diffuse *= shadow_fac;
    final_spec    *= shadow_fac;
    
    
#ifdef LIGHT_MAP
    vec3 lm = texture2D( light_texture, gl_TexCoord[1].xy ).rgb;
#ifdef EMISSIVE_MAP
    lm = min(vec3(1.0), lm + vec3(em));
#endif
    final_diffuse *= lm;
    final_spec    *= lm;
#endif // LIGHT_MAP

    final_diffuse += gl_LightSource[0].ambient.rgb;

    
#ifdef EMISSIVE_MAP
    final_diffuse = max(final_diffuse, vec3(em));
#endif
    
    gl_FragColor.rgb = gl_FragColor.rgb*final_diffuse + final_spec;
    
#endif // LIGHTING
    
    gl_FragColor.rgb = mix(gl_Fog.color.rgb, gl_FragColor.rgb, fog_factor);
}



uniform sampler2D reflection_texture;
uniform sampler2D dudv_texture;
uniform sampler2D normal_texture;
uniform float elapsed_time;


varying float fog_factor;

varying vec4 proj_view_coords;
varying vec2 tex_coords_norm;
varying vec2 tex_coords_dudv;
varying vec3 light_dir_tangentspace;
varying vec3 eye_dir_tangentspace;

//------------------------------------------------------------------------------
void main() 
{

    /* parameter values */
    const float refraction_factor = 0.009;
    const float distortion_factor = 0.009;
    const float wave_speed = 0.02;
    const float noise_speed = 0.013;     

    const float refract_bias = 0.1;           // affects mixing of water_color + reflection
    const float refract_power = 2.0;        // affects mixing of water_color + reflection
    const float specular_factor = 540.0;
    const float specular_brightness = 0.9;
    
    const vec3 water_color = vec3(0.0, 0.2, 0.7);    
    
    vec2 tex_coords_dudvmap = tex_coords_dudv; 
    vec2 tex_coords_normalmap = tex_coords_norm * 2.7;  // bring size on normal tex

    vec4 dudv_offset = texture2D(dudv_texture, tex_coords_dudvmap.xy) * distortion_factor;

    /* animate dudv texture */
    tex_coords_dudvmap.x += wave_speed * elapsed_time; 
    tex_coords_dudvmap.y += wave_speed * elapsed_time;

    /* animate normal texture */
    tex_coords_normalmap.x += noise_speed * elapsed_time; 
    tex_coords_normalmap.y += noise_speed * elapsed_time;

    vec4 dudv_color = texture2D(dudv_texture, tex_coords_dudvmap + dudv_offset.st); // in test with + distOffset.st 
    dudv_color = normalize(dudv_color * 2.0 - 1.0) * refraction_factor;


    // der normalvector kommt aus der normalmap und wird mir x2-1 in den bereich [1,0] gebracht 
    vec3 normal_vector = texture2D( normal_texture, tex_coords_normalmap + dudv_color.st ).xzy;  /// caution z is up in normal map + distOffset.st
    normal_vector = normalize(normal_vector * 2.0 - 1.0);


#ifdef REFLECTIONS
    // convert projective texture coordinates into normal texture coordinates 
    vec4 refl_tex_coords = proj_view_coords / proj_view_coords.w;
    refl_tex_coords += dudv_color;
    refl_tex_coords = clamp(refl_tex_coords, 0.001, 0.999);        
#else
    // plain normal tex coords from water texture, disturbed a bit
    vec4 refl_tex_coords = gl_TexCoord[0] - wave_speed * elapsed_time; // animate texture
    refl_tex_coords += dudv_color;
#endif 
    
    vec3 reflection_color = texture2D(reflection_texture, refl_tex_coords.st).rgb;      

    // mix water color and reflection color based on fresnelTerm (view angle)
    float fresnel_factor = refract_bias+pow(max(dot(normalize(eye_dir_tangentspace), -normal_vector), 0.0), refract_power);
    vec3 final_water_color = mix(reflection_color, water_color, fresnel_factor);

    vec3 reflect_vector = reflect(normalize(light_dir_tangentspace), normal_vector);
    vec3 specular   = vec3(pow(max(dot(normalize(eye_dir_tangentspace), reflect_vector), 0.0),  specular_factor)) * specular_brightness;

    float alpha = 1.0 - clamp(fresnel_factor, 0.1, 0.5);
    gl_FragColor = vec4(final_water_color  + specular , alpha);

    gl_FragColor.rgb = mix(gl_Fog.color.rgb, gl_FragColor.rgb, fog_factor);    
}

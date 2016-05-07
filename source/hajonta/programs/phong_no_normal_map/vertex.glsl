#version 330

uniform mat4 u_projection;
uniform mat4 u_view_matrix;
uniform mat4 u_model_matrix;
uniform mat4 u_lightspace_matrix;

uniform mat4 u_bones[100];

uniform int u_lightspace_available;
uniform int u_shadow_mode;
uniform int u_poisson_spread;
uniform float u_bias;
uniform int u_pcf_distance;
uniform int u_poisson_samples;
uniform int u_bones_enabled;
uniform float u_poisson_position_granularity;
uniform float u_minimum_variance;
uniform float u_lightbleed_compensation;
uniform vec3 u_camera_position;

in vec3 a_position;
in vec2 a_texcoord;
in vec3 a_normal;
in ivec4 a_bone_ids;
in vec4 a_bone_weights;

out vec3 v_w_position;
out vec2 v_texcoord;
out vec3 v_w_normal;
out vec4 v_l_position;
flat out ivec4 v_bone_ids;
out vec4 v_bone_weights;

void main()
{
    mat4 bone_transform = mat4(0);
    if (u_bones_enabled > 0)
    {
        bone_transform += u_bones[a_bone_ids[0]] * a_bone_weights[0];
        bone_transform += u_bones[a_bone_ids[1]] * a_bone_weights[1];
        bone_transform += u_bones[a_bone_ids[2]] * a_bone_weights[2];
        bone_transform += u_bones[a_bone_ids[3]] * a_bone_weights[3];
    }
    else
    {
        bone_transform = mat4(1);
    }
    v_texcoord = a_texcoord;
    vec4 post_bone_normal = bone_transform * vec4(a_normal, 1.0);
    vec4 post_bone_position = bone_transform * vec4(a_position, 1.0);

    v_w_normal = mat3(transpose(inverse(u_model_matrix))) * (post_bone_normal.xyz / post_bone_normal.w);
    vec4 w_position = u_model_matrix * post_bone_position;
    v_w_position = w_position.xyz / w_position.w;
    if (u_lightspace_available == 1)
    {
        v_l_position = u_lightspace_matrix * w_position;
    }
    gl_Position = u_projection * u_view_matrix * w_position;
    v_bone_ids = a_bone_ids;
    v_bone_weights = a_bone_weights;
}

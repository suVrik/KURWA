#define PI 3.14159265

struct FS_INPUT {
    float4 position : SV_POSITION;
};

Texture2D albedo_ao_uniform_attachment;
Texture2D normal_roughness_uniform_attachment;
Texture2D emission_metalness_uniform_attachment;
Texture2D depth_uniform_attachment;

TextureCube shadow_uniform_texture;
Texture3D pcf_rotation_uniform_texture;

SamplerState sampler_uniform;
SamplerComparisonState shadow_sampler_uniform;

cbuffer LightUniformBuffer {
    float4x4 view_projection;
    float4x4 inverse_view_projection;
    float4 view_position;
    float2 texel_size;
    float2 specular_diffuse;
};

struct SphereLightPushConstants {
    float4 position;
    float4 luminance;

    // x goes for light radius.
    // y goes for attenuation radius.
    // z goes for light's Z near.
    // w goes for light's Z far.
    float4 radius_frustum;

    // x goes for normal bias.
    // y goes for perspective bias.
    // z goes for pcss radius factor.
    // w goes for pcss filter factor.
    float4 shadow_params;
};

[[vk::push_constant]] SphereLightPushConstants sphere_light_push_constants;

static const float2 POISSON_DISK[16] = {
    float2(-0.94201624, -0.39906216),
    float2( 0.94558609, -0.76890725),
    float2(-0.09418410, -0.92938870),
    float2( 0.34495938,  0.29387760),
    float2(-0.91588581,  0.45771432),
    float2(-0.81544232, -0.87912464),
    float2(-0.38277543,  0.27676845),
    float2( 0.97484398,  0.75648379),
    float2( 0.44323325, -0.97511554),
    float2( 0.53742981, -0.47373420),
    float2(-0.26496911, -0.41893023),
    float2( 0.79197514,  0.19090188),
    float2(-0.24188840,  0.99706507),
    float2(-0.81409955,  0.91437590),
    float2( 0.19984126,  0.78641367),
    float2( 0.14383161, -0.14100790)
};

float pcss(float3 light_position, float3 clip_position, float3 clip_normal, float light_radius) {
    float3 light_clip_vector = clip_position + clip_normal * sphere_light_push_constants.shadow_params.x - light_position;

    float z_near = sphere_light_push_constants.radius_frustum.z;
    float z_far = sphere_light_push_constants.radius_frustum.w;

    float3 abs_light_clip_vector = abs(light_clip_vector);
    float linear_depth = max(abs_light_clip_vector.x, max(abs_light_clip_vector.y, abs_light_clip_vector.z));
    float non_linear_depth = z_far / (z_far - z_near) + (z_far * z_near) / (linear_depth * (z_near - z_far));

    float3 disk_basis_z = normalize(light_clip_vector);
    float3 temp = normalize(cross(disk_basis_z, float3(0.24188840, 0.90650779, 0.34602550)));
    float3 disk_basis_x = cross(disk_basis_z, temp);
    float3 disk_basis_y = cross(disk_basis_z, disk_basis_x);

    float2 rotation = pcf_rotation_uniform_texture.Sample(sampler_uniform, clip_position * 100.0).xy;

    float light_frustum_width = z_far * 2.0;
    float light_radius_uv = light_radius * sphere_light_push_constants.shadow_params.z / light_frustum_width;
    float search_radius = light_radius_uv * (linear_depth - z_near) / linear_depth;

    float sum = 0.0;
    int count = 0;

    for (int i = 0; i < 16; i++) {
        float poisson_disk_x = POISSON_DISK[i].x * rotation.x - POISSON_DISK[i].y * rotation.y;
        float poisson_disk_y = POISSON_DISK[i].y * rotation.y + POISSON_DISK[i].y * rotation.x;

        float3 uv = light_clip_vector + search_radius * (disk_basis_x * poisson_disk_x + disk_basis_y * poisson_disk_y);

        float non_linear_blocker_depth = shadow_uniform_texture.SampleLevel(sampler_uniform, uv, 0.0).r;
        if (non_linear_blocker_depth < non_linear_depth - sphere_light_push_constants.shadow_params.y) {
            sum += non_linear_blocker_depth;
            count++;
        }
    }

    if (count == 0) {
        return 1.0;
    }

    float average_linear_blocker_depth = z_far * z_near / (z_far - sum / count * (z_far - z_near));
    float filter_radius = search_radius * saturate((linear_depth - average_linear_blocker_depth) * sphere_light_push_constants.shadow_params.w);

    sum = 0;

    for (int i = 0; i < 16; i++) {
        float poisson_disk_x = POISSON_DISK[i].x * rotation.x - POISSON_DISK[i].y * rotation.y;
        float poisson_disk_y = POISSON_DISK[i].y * rotation.y + POISSON_DISK[i].y * rotation.x;

        float3 uv = light_clip_vector + filter_radius * (disk_basis_x * poisson_disk_x + disk_basis_y * poisson_disk_y);

        sum += shadow_uniform_texture.SampleCmpLevelZero(shadow_sampler_uniform, uv, non_linear_depth - sphere_light_push_constants.shadow_params.y).r;
    }

    return sum / 16.0;
}

float specular_d(float n_dot_h, float sqr_alpha) {
    float denominator = n_dot_h * n_dot_h * (sqr_alpha - 1.0) + 1.0;
    float sqr_denominator = denominator * denominator;
    return sqr_alpha / sqr_denominator;
}

float3 specular_f(float v_dot_h, float3 f0) {
    return f0 + (1.0 - f0) * pow(1.0 - v_dot_h, 5.0);
}

float specular_g1(float n_dot_x, float k) {
    return n_dot_x / (n_dot_x * (1.0 - k) + k);
}

float specular_g(float n_dot_l, float n_dot_v, float roughness) {
    float alpha = (roughness + 1.0) / 2.0;
    float k = alpha * alpha / 2.0;
    return specular_g1(n_dot_l, k) * specular_g1(n_dot_v, k);
}

float integrate_sphere(float radius_over_distance, float normal_dot_light) {
    float sqr_radius_over_distance = radius_over_distance * radius_over_distance;
    float one_sqr_radius_over_distance = 1.0 + sqr_radius_over_distance;
    float form_factor = sqr_radius_over_distance / sqrt(one_sqr_radius_over_distance * one_sqr_radius_over_distance);
    return form_factor * max((form_factor * form_factor + normal_dot_light) / (form_factor + 1.0), 0.0);
}

float attenuation(float distance, float attenuation_radius) {
    float factor = distance / attenuation_radius;
    float sqr_factor = factor * factor;
    float result = saturate(1.0 - sqr_factor * sqr_factor);
    float sqr_result = result * result;
    return sqr_result;
}

float4 main(FS_INPUT input) : SV_TARGET {
    float2 texcoord = input.position.xy * texel_size.xy;
    float2 screen_position = texcoord.xy * float2(2.0, -2.0) - float2(1.0, -1.0);

    float4 albedo_ao_sample = albedo_ao_uniform_attachment.Sample(sampler_uniform, texcoord);
    float4 normal_roughness_sample = normal_roughness_uniform_attachment.Sample(sampler_uniform, texcoord);
    float4 emission_metalness_sample = emission_metalness_uniform_attachment.Sample(sampler_uniform, texcoord);
    float4 depth_sample = depth_uniform_attachment.Sample(sampler_uniform, texcoord);

    float3 albedo = albedo_ao_sample.rgb;
    float ambient_occlusion = albedo_ao_sample.a;
    float3 normal_direction = normal_roughness_sample.rgb;
    float roughness = max(normal_roughness_sample.a, 0.075);
    float3 emission = emission_metalness_sample.rgb;
    float metalness = emission_metalness_sample.a;
    float depth = depth_sample.r;

    float4 clip_position = mul(inverse_view_projection, float4(screen_position.x, screen_position.y, depth, 1.0));
    clip_position /= clip_position.w;

    float3 light_luminance = sphere_light_push_constants.luminance.xyz;
    float2 light_radius = sphere_light_push_constants.radius_frustum.xy;

    float3 view_direction = normalize(view_position.xyz - clip_position.xyz);
    float3 reflection_direction = normalize(reflect(-view_direction, normal_direction));

    float3 light_center_vector = sphere_light_push_constants.position.xyz - clip_position.xyz;
    float light_center_distance = length(light_center_vector);
    float3 light_center_direction = light_center_vector / light_center_distance;

    float3 light_center_to_reflection = dot(light_center_vector, reflection_direction) * reflection_direction - light_center_vector;

    float3 light_direction = light_center_vector + light_center_to_reflection * saturate(light_radius.x / length(light_center_to_reflection));
    float light_distance = length(light_direction);
    light_direction /= light_distance;

    float3 halfway_direction = normalize(view_direction + light_direction);

    float n_dot_l = saturate(dot(normal_direction, light_direction));
    float n_dot_v = saturate(dot(normal_direction, view_direction));
    float n_dot_h = saturate(dot(normal_direction, halfway_direction));
    float v_dot_h = saturate(dot(view_direction, halfway_direction));

    float alpha = roughness * roughness;
    float sqr_alpha = alpha * alpha;
    float3 f0 = lerp(float3(0.04, 0.04, 0.04), albedo, metalness);
    float energy_normalization = saturate(alpha + light_radius.x / (light_center_distance * 3.0));

    float  d = specular_d(n_dot_h, sqr_alpha);
    float3 f = specular_f(v_dot_h, f0);
    float  g = specular_g(n_dot_l, n_dot_v, roughness);

    float3 specular = max(d * f * g * energy_normalization / (4.0 * n_dot_l * n_dot_v) * specular_diffuse.x, 0.0);

    float radius_over_distance = light_radius.x / light_center_distance;
    float normal_dot_light_center = dot(normal_direction, light_center_direction);

    float3 diffuse = (1.0 - f) * (1.0 - metalness) * albedo * ambient_occlusion * integrate_sphere(radius_over_distance, normal_dot_light_center) * specular_diffuse.y;

    float shadow_sample = pcss(sphere_light_push_constants.position.xyz, clip_position.xyz, normal_direction, sphere_light_push_constants.radius_frustum.x);
    
    return float4((specular + diffuse) * attenuation(light_center_distance, light_radius.y) * light_luminance * shadow_sample, 1.0);
}

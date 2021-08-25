#define SAMPLE_COUNT 13u

struct FS_INPUT {
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD;
};

Texture2D input_uniform_attachment;
SamplerState sampler_uniform;

struct DownsamplingPushConstants {
    float4 texel_size;
};

[[vk::push_constant]] DownsamplingPushConstants downsampling_push_constants;

static const float3 SAMPLES[SAMPLE_COUNT] = {
    float3(-1.0, -1.0, 0.03125),
    float3( 0.0, -1.0,  0.0625),
    float3( 1.0, -1.0, 0.03125),
    float3(-0.5, -0.5,   0.125),
    float3( 0.5, -0.5,   0.125),
    float3(-1.0,  0.0, 0.03125),
    float3( 0.0,  0.0,   0.125),
    float3( 1.0,  0.0, 0.03125),
    float3(-0.5,  0.5,   0.125),
    float3( 0.5,  0.5,   0.125),
    float3(-1.0,  1.0, 0.03125),
    float3( 0.0,  1.0,  0.0625),
    float3( 1.0,  1.0, 0.03125),
};

float4 main(FS_INPUT input) : SV_TARGET {
    float3 result = float3(0.0, 0.0, 0.0);
    for (uint i = 0; i < SAMPLE_COUNT; i++) {
        result += input_uniform_attachment.Sample(sampler_uniform, input.texcoord + SAMPLES[i].xy * downsampling_push_constants.texel_size.xy).xyz * SAMPLES[i].z;
    }
    return float4(result, 1.0);
}

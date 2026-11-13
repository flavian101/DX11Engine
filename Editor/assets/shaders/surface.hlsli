// surface.hlsli
//
// ONE place that turns "a material + its textures + the interpolated vertex data" into a SURFACE, and one place that
// lights a surface. Every shading model that reads a material (Lit, Emissive, Unlit) calls EvaluateSurface(), so the
// rules below hold for all of them. Before this file Lit.ps, PBR.ps and Transparent.ps each had their own copy, and the
// copies disagreed:
//   - the same texture was converted to linear in one and not in the others (fixed with the linear workflow);
//   - alpha was multiplied in twice in some shaders and once in others;
//   - a plain emissiveColor (no emissive map) was ignored by Lit / PBR;
//   - the roughness map REPLACED the material roughness in Lit and MULTIPLIED it in PBR;
//   - the occlusion strength was applied twice in PBR and never in Lit.
//
// THE RULES (glTF conventions: a material value is a FACTOR, a texture multiplies it, nothing is applied twice)
//   baseColor : diffuseColor.rgb x diffuse texture x detail diffuse x vertex colour           (linear)
//   alpha     : diffuseColor.a x alpha x diffuse texture.a x opacity map x vertex colour.a
//   roughness : roughness x roughness map; or x the packed Specular map's G when there is no dedicated map
//   metallic  : metallic x metallic map;    or x the packed Specular map's B when there is no dedicated map
//   occlusion : lerp(1, ao, occlusionStrength), ao = packed Specular R x AO map
//   emissive  : emissiveColor.rgb x emissiveIntensity x emissive map                          (always, map or not)
//   normal    : the vertex normal, bent by the normal map when there is one (needs tangents)
// Colour textures arrive LINEAR (they are sRGB textures); data textures are raw numbers. See the note in common.hlsli.
//
// Include after common.hlsli.

#ifndef SURFACE_HLSLI
#define SURFACE_HLSLI

// Pixels with alpha below this are discarded when ENABLE_ALPHA_TEST is on. 0.5 is the cut-out convention (leaves, fences);
// a BLENDED material only wants to skip pixels that would be invisible anyway: ShaderCache then defines this as 0.1 (the Blended feature).
#ifndef ALPHA_TEST_THRESHOLD
    #define ALPHA_TEST_THRESHOLD 0.5
#endif

struct Surface
{
    float3 baseColor;      // linear
    float  alpha;
    float3 normal;         // world space, normalized
    float  metallic;       // 0..1
    float3 emissive;       // linear radiance
    float  roughness;      // MIN_ROUGHNESS..1
    float  occlusion;      // 0..1, the strength already applied
    float2 uv;             // the texture coordinate AFTER parallax (for any further sampling)
};

Surface EvaluateSurface(StandardVertexOutput input, float3 V)
{
    Surface s;

    // ---------------- texture coordinate (parallax moves it before anything is sampled) ----------------
    float2 uv = float2(0.0, 0.0);
#if HAS_TEXCOORDS_ATTRIBUTE
    uv = input.texCoord;
#if ENABLE_PARALLAX_MAPPING && HAS_HEIGHT_MAP && HAS_TANGENT_ATTRIBUTE
    uv = ParallaxOcclusionMapping(uv, V, input.normal, input.tangent);
#endif
#endif
    s.uv = uv;

    // ---------------- factors from the material buffer ----------------
    float4 baseColor = diffuseColor;
    float  opacity   = alpha;
    float  roughnessValue = roughness;
    float  metallicValue  = metallic;
    float  ao = 1.0;
    float3 emissiveValue = emissiveColor.rgb * emissiveIntensity;

    // ---------------- textures multiply the factors ----------------
#if HAS_TEXCOORDS_ATTRIBUTE
#if HAS_DIFFUSE_TEXTURE
    baseColor *= SampleDiffuseTexture(uv);
#endif

#if HAS_DETAIL_TEXTURES && HAS_DETAIL_DIFFUSE_MAP
    float4 detail = SampleDetailDiffuse(uv);
    baseColor.rgb = lerp(baseColor.rgb, baseColor.rgb * detail.rgb, detail.a * 0.5);
#endif

#if HAS_SPECULAR_MAP && !HAS_METALLIC_MAP && !HAS_ROUGHNESS_MAP
    // packed workflow: R = ambient occlusion, G = roughness, B = metallic
    float3 packed = SampleSpecularMap(uv);
    ao             *= packed.r;
    roughnessValue *= packed.g;
    metallicValue  *= packed.b;
#endif

#if HAS_ROUGHNESS_MAP
    roughnessValue *= SampleRoughnessMap(uv);
#endif
#if HAS_METALLIC_MAP
    metallicValue *= SampleMetallicMap(uv);
#endif
#if HAS_AO_MAP
    ao *= SampleAOMap(uv);
#endif
#if HAS_EMISSIVE_MAP
    emissiveValue *= SampleEmissiveMap(uv);
#endif
#if HAS_OPACITY_MAP
    opacity *= SampleOpacityMap(uv);
#endif
#endif // HAS_TEXCOORDS_ATTRIBUTE

    // ---------------- vertex colour ----------------
#if HAS_VERTEX_COLOR_ATTRIBUTE
    baseColor *= input.color;
#endif

    // ---------------- normal ----------------
#if HAS_NORMAL_ATTRIBUTE
    float3 N = normalize(input.normal);
#else
    float3 N = float3(0.0, 0.0, 1.0);
#endif
#if HAS_NORMAL_MAP && HAS_TANGENT_ATTRIBUTE && HAS_TEXCOORDS_ATTRIBUTE
    N = CalculateWorldNormal(N, input.tangent, uv);
#endif

    // ---------------- result ----------------
    s.baseColor = baseColor.rgb;
    s.alpha     = baseColor.a * opacity;
    s.normal    = N;
    s.metallic  = saturate(metallicValue);
    s.roughness = clamp(roughnessValue, MIN_ROUGHNESS, 1.0);       // a perfectly smooth surface breaks the specular lobe
    s.occlusion = lerp(1.0, saturate(ao), occlusionStrength);
    s.emissive  = emissiveValue;
    return s;
}

// ==========================================================================================================
// Lighting: ambient + every directional / local light + image-based lighting + emissive + fog.
// Returns LINEAR HDR colour. Tone mapping and the display encoding are the caller's last step (ToDisplay).
// ==========================================================================================================
float3 ShadeSurface(Surface s, StandardVertexOutput input, float3 V)
{
    const float3 N = s.normal;

    // dielectrics reflect ~4% at normal incidence, metals reflect their colour; metals have no diffuse (energy conservation)
    const float3 F0     = lerp(float3(0.04, 0.04, 0.04), s.baseColor, s.metallic);
    const float3 albedo = s.baseColor * (1.0 - s.metallic);

    float3 color = ambientColor * ambientIntensity * s.baseColor * s.occlusion;

    [loop]
    for (uint i = 0; i < directionalLightCount; ++i)
        color += CalculateDirectionalLight(directionalLights[i], N, V, albedo, s.metallic, s.roughness, F0, input.worldPos);

    color += ShadeLocalLights(input.position, input.worldPos.xyz, N, V, albedo, s.metallic, s.roughness, F0);

#if HAS_ENVIRONMENT_MAP
    // reflections from the environment cube map
    const float  NdotV = max(dot(N, V), 0.0);
    const float3 R = reflect(-V, N);
    const float3 prefiltered = environmentTexture.SampleLevel(standardSampler, R, s.roughness * 8.0).rgb;
    const float3 F_env = FresnelSchlickRoughness(NdotV, F0, s.roughness);
    const float3 kD_env = (1.0 - F_env) * (1.0 - s.metallic);

#if HAS_IRRADIANCE_MAP
    const float3 diffuseIBL = kD_env * irradianceTexture.Sample(standardSampler, N).rgb * s.baseColor;
#else
    const float3 diffuseIBL = float3(0.0, 0.0, 0.0);
#endif

    // simplified split-sum BRDF (no lookup table yet)
    const float2 envBRDF = float2(1.0 - s.roughness, 1.0) * NdotV;
    const float3 specularIBL = prefiltered * (F_env * envBRDF.x + envBRDF.y);
    color += (diffuseIBL + specularIBL) * iblIntensity;
#endif

    color += s.emissive;

#if ENABLE_FOG
    const float fogFactor = saturate(exp(-length(input.viewDir) * 0.001));
    color = lerp(ambientColor, color, fogFactor);
#endif

    return color;
}

// HDR linear -> what goes to the (non-sRGB) back buffer: exposure tone mapping, then the display encoding.
float3 ToDisplay(float3 linearColor)
{
    return ApplyGamma(ToneMapExposure(linearColor, exposure), gamma);
}

#endif // SURFACE_HLSLI

#include <vsg/io/VSG.h>
static auto fbxshader_frag = []() {std::istringstream str(
R"(#vsga 0.5.4
Root id=1 vsg::ShaderStage
{
  userObjects 0
  stage 16
  entryPointName "main"
  module id=2 vsg::ShaderModule
  {
    userObjects 0
    hints id=0
    source "#version 450
#pragma import_defines ( VSG_NORMAL, VSG_COLOR, VSG_TEXCOORD0, VSG_LIGHTING, VSG_DIFFUSE_MAP, VSG_NORMAL_MAP, VSG_AORM_MAP )
#extension GL_ARB_separate_shader_objects : enable

const float PI = 3.14159265359;
const float RECIPROCAL_PI = 0.31830988618;
const float RECIPROCAL_PI2 = 0.15915494;
const float EPSILON = 1e-6;
const float c_MinRoughness = 0.04;

#ifdef VSG_DIFFUSE_MAP
layout(binding = 0) uniform sampler2D diffuseMap;
#endif

#ifdef VSG_NORMAL_MAP
layout(binding = 5) uniform sampler2D normalMap;
#endif

#ifdef VSG_AORM_MAP
layout(binding = 8) uniform sampler2D aormMap;
#endif

#ifdef VSG_NORMAL
layout(location = 1) in vec3 normalDir;
#endif

#ifdef VSG_COLOR
layout(location = 3) in vec4 vertexColor;
#endif

#ifdef VSG_TEXCOORD0
layout(location = 4) in vec2 texCoord0;
#endif

#ifdef VSG_LIGHTING
layout(location = 2) in vec3 eyePos;
layout(location = 5) in vec3 viewDir;
layout(location = 6) in vec3 lightDir;
#endif

layout(location = 0) out vec4 outColor;

layout(set = 1, binding = 0) uniform LightData
{
    vec4 values[2048];
} lightData;

// -------------------------------------------------------------------------------------------

struct PBRInfo
{
    float NdotL;                  // cos angle between normal and light direction
    float NdotV;                  // cos angle between normal and view direction
    float NdotH;                  // cos angle between normal and half vector
    float LdotH;                  // cos angle between light direction and half vector
    float VdotH;                  // cos angle between view direction and half vector
    float VdotL;                  // cos angle between view direction and light direction
    float perceptualRoughness;    // roughness value, as authored by the model creator (input to shader)
    float metalness;              // metallic value at the surface
    vec3 reflectance0;            // full reflectance color (normal incidence angle)
    vec3 reflectance90;           // reflectance color at grazing angle
    float alphaRoughness;         // roughness mapped to a more linear change in the roughness (proposed by [2])
    vec3 diffuseColor;            // color contribution from diffuse lighting
    vec3 specularColor;           // color contribution from specular lighting
};

// -------------------------------------------------------------------------------------------

vec3 getNormal()
{
    vec3 result = vec3(0.0);
	
#ifdef VSG_NORMAL_MAP
	vec2 uv = texCoord0;
	uv.y = 1 - uv.y;
	
    vec3 tangentNormal = texture(normalMap, uv).xyz * 2.0 - 1.0;
	
    vec3 q1 = dFdx(eyePos);
    vec3 q2 = dFdy(eyePos);
    vec2 st1 = dFdx(texCoord0);
    vec2 st2 = dFdy(texCoord0);

    vec3 N = normalize(normalDir);
    vec3 T = normalize(q1 * st2.t - q2 * st1.t);
    vec3 B = -normalize(cross(N, T));
    mat3 TBN = mat3(T, B, N);

    result = normalize(TBN * tangentNormal);
#else
	#ifdef VSG_NORMAL
    result = normalize(normalDir);
	#endif
#endif

    return result;
}

vec4 SRGBtoLINEAR(vec4 srgbIn)
{
    vec3 linOut = pow(srgbIn.xyz, vec3(2.2));
    return vec4(linOut,srgbIn.w);
}

vec4 LINEARtoSRGB(vec4 srgbIn)
{
    vec3 linOut = pow(srgbIn.xyz, vec3(1.0 / 2.2));
    return vec4(linOut, srgbIn.w);
}

vec3 specularReflection(PBRInfo pbrInputs)
{
    //return pbrInputs.reflectance0 + (pbrInputs.reflectance90 - pbrInputs.reflectance0) * pow(clamp(1.0 - pbrInputs.VdotH, 0.0, 1.0), 5.0);
    return pbrInputs.reflectance0 + (pbrInputs.reflectance90 - pbrInputs.reflectance90*pbrInputs.reflectance0) * exp2((-5.55473 * pbrInputs.VdotH - 6.98316) * pbrInputs.VdotH);
}

float geometricOcclusion(PBRInfo pbrInputs)
{
    float NdotL = pbrInputs.NdotL;
    float NdotV = pbrInputs.NdotV;
    float r = pbrInputs.alphaRoughness * pbrInputs.alphaRoughness;

    float attenuationL = 2.0 * NdotL / (NdotL + sqrt(r + (1.0 - r) * (NdotL * NdotL)));
    float attenuationV = 2.0 * NdotV / (NdotV + sqrt(r + (1.0 - r) * (NdotV * NdotV)));
    return attenuationL * attenuationV;
}

float microfacetDistribution(PBRInfo pbrInputs)
{
    float roughnessSq = pbrInputs.alphaRoughness * pbrInputs.alphaRoughness;
    float f = (pbrInputs.NdotH * roughnessSq - pbrInputs.NdotH) * pbrInputs.NdotH + 1.0;
    return roughnessSq / (PI * f * f);
}

vec3 BRDF_Diffuse_Disney(PBRInfo pbrInputs)
{
	float Fd90 = 0.5 + 2.0 * pbrInputs.perceptualRoughness * pbrInputs.VdotH * pbrInputs.VdotH;
    vec3 f0 = vec3(0.1);
	vec3 invF0 = vec3(1.0, 1.0, 1.0) - f0;
	float dim = min(invF0.r, min(invF0.g, invF0.b));
	float result = ((1.0 + (Fd90 - 1.0) * pow(1.0 - pbrInputs.NdotL, 5.0 )) * (1.0 + (Fd90 - 1.0) * pow(1.0 - pbrInputs.NdotV, 5.0 ))) * dim;
	return pbrInputs.diffuseColor * result;
}

vec3 BRDF(vec3 u_LightColor, vec3 v, vec3 n, vec3 l, vec3 h, float perceptualRoughness, float metallic, vec3 specularEnvironmentR0, vec3 specularEnvironmentR90, float alphaRoughness, vec3 diffuseColor, vec3 specularColor, float ao)
{
    float unclmapped_NdotL = dot(n, l);

    vec3 reflection = -normalize(reflect(v, n));
    reflection.y *= -1.0f;

    float NdotL = clamp(unclmapped_NdotL, 0.001, 1.0);
    float NdotV = clamp(abs(dot(n, v)), 0.001, 1.0);
    float NdotH = clamp(dot(n, h), 0.0, 1.0);
    float LdotH = clamp(dot(l, h), 0.0, 1.0);
    float VdotH = clamp(dot(v, h), 0.0, 1.0);
    float VdotL = clamp(dot(v, l), 0.0, 1.0);

    PBRInfo pbrInputs = PBRInfo(NdotL,
                                NdotV,
                                NdotH,
                                LdotH,
                                VdotH,
                                VdotL,
                                perceptualRoughness,
                                metallic,
                                specularEnvironmentR0,
                                specularEnvironmentR90,
                                alphaRoughness,
                                diffuseColor,
                                specularColor);

    // Calculate the shading terms for the microfacet specular shading model
    vec3 F = specularReflection(pbrInputs);
    float G = geometricOcclusion(pbrInputs);
    float D = microfacetDistribution(pbrInputs);

    // Calculation of analytical lighting contribution
    vec3 diffuseContrib = (1.0 - F) * BRDF_Diffuse_Disney(pbrInputs);
    vec3 specContrib = F * G * D / (4.0 * NdotL * NdotV);
	
    // Obtain final intensity as reflectance (BRDF) scaled by the energy of the light (cosine law)
    vec3 color = NdotL * u_LightColor * (diffuseContrib + specContrib);

    color *= ao;

    return color;
}

// -------------------------------------------------------------------------------------------

void main()
{
    vec4 lightNums = lightData.values[0];
    int numAmbientLights = int(lightNums[0]);
    int numDirectionalLights = int(lightNums[1]);
    int numPointLights = int(lightNums[2]);
    int numSpotLights = int(lightNums[3]);
	
	vec4 pbrBaseColorFactor = vec4(1.0);
	float pbrRoughnessFactor = 0.7;
	float pbrMetallicFactor = 0.01;
	
    float brightnessCutoff = 0.001;
    float perceptualRoughness = 0.0;
    float metallic = 0.0;
    float ambientOcclusion = 1.0;
	
    vec4 baseColor;
    vec3 diffuseColor;
    vec3 f0 = vec3(0.04);
	
#ifdef VSG_COLOR
	vec4 inputColor = vertexColor;
#else
	vec4 inputColor = vec4(1.0);
#endif
	
#ifdef VSG_DIFFUSE_MAP
    baseColor = inputColor * SRGBtoLINEAR(texture(diffuseMap, texCoord0)) * pbrBaseColorFactor;
#else
    baseColor = inputColor * pbrBaseColorFactor;
#endif
	
#ifdef VSG_AORM_MAP
	perceptualRoughness = pbrRoughnessFactor;
	metallic = pbrMetallicFactor;

	vec3 aormData = texture(aormMap, texCoord0).xyz;
	
    ambientOcclusion = aormData.r;
	perceptualRoughness = aormData.g * perceptualRoughness;
	metallic = aormData.b * metallic;
#endif
	
    vec3 color = vec3(0.0, 0.0, 0.0);
	vec4 ambient_color = vec4(1.0, 1.0, 1.0, 0.75);
	
#ifdef VSG_LIGHTING
    diffuseColor = baseColor.rgb * (vec3(1.0) - f0);
    diffuseColor *= 1.0 - metallic;
	
    float alphaRoughness = perceptualRoughness * perceptualRoughness;
	
    vec3 specularColor = mix(f0, baseColor.rgb, metallic);
	
    // Compute reflectance.
    float reflectance = max(max(specularColor.r, specularColor.g), specularColor.b);
	float reflectance90 = clamp(reflectance * 25.0, 0.0, 1.0);
    vec3 specularEnvironmentR0 = specularColor.rgb;
    vec3 specularEnvironmentR90 = vec3(1.0, 1.0, 1.0) * reflectance90;
	
	vec4 lightColor = vec4(1.0);
    vec3 n = getNormal();
    vec3 v = normalize(viewDir);
	vec3 l = -lightDir;
	vec3 h = normalize(l+v); 
	
	float scale = lightColor.a;
	
	color.rgb += BRDF(lightColor.rgb * scale, v, n, l, h, 
					  perceptualRoughness, metallic, specularEnvironmentR0, specularEnvironmentR90, 
					  alphaRoughness, diffuseColor, specularColor, ambientOcclusion);
        
#endif

	color += (baseColor.rgb * ambient_color.rgb) * (ambient_color.a * ambientOcclusion);

	outColor = LINEARtoSRGB(vec4(color, baseColor.a));
	
    if (outColor.a==0.0)
		discard;
}

"
    code 0
    
  }
  NumSpecializationConstants 0
}
)");
vsg::VSG io;
return io.read_cast<vsg::ShaderStage>(str);
};

#version 450

/////////////////////////////////////////////////////////////////////////////////////
// CONSTANTS
/////////////////////////////////////////////////////////////////////////////////////
#define MAX_POINT_LIGHTS 10
#define MAX_SPOT_LIGHTS 10

#define CASCADE_SHADOW_MAP_COUNT 4

const float epsilon = 0.00001;
const float PI = 3.14159265359;

const vec3 gridSamplingDisk[20] = vec3[]
(
   vec3(1, 1,  1), vec3( 1, -1,  1), vec3(-1, -1,  1), vec3(-1, 1,  1), 
   vec3(1, 1, -1), vec3( 1, -1, -1), vec3(-1, -1, -1), vec3(-1, 1, -1),
   vec3(1, 1,  0), vec3( 1, -1,  0), vec3(-1, -1,  0), vec3(-1, 1,  0),
   vec3(1, 0,  1), vec3(-1,  0,  1), vec3( 1,  0, -1), vec3(-1, 0, -1),
   vec3(0, 1,  1), vec3( 0, -1,  1), vec3( 0, -1, -1), vec3( 0, 1, -1)
);

const mat4 biasMat = mat4( 
  0.5, 0.0, 0.0, 0.0,
  0.0, 0.5, 0.0, 0.0,
  0.0, 0.0, 1.0, 0.0,
  0.5, 0.5, 0.0, 1.0 );
/////////////////////////////////////////////////////////////////////////////////////
// CONSTANTS
/////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////
// FRAGMENT INPUT
/////////////////////////////////////////////////////////////////////////////////////
layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec3 fragModelWorldSpace; // outEyePos
layout(location = 2) in vec3 fragNormalWorldSpace;
layout(location = 3) in vec3 fragTangent;
layout(location = 4) in vec2 fragUV;
layout(location = 5) in vec4 fragViewPos;

layout(location = 7) in vec3 fragModelPos; //outWorldPos
layout(location = 8) in vec3 fragLightVec; //outLightVec

layout(location = 9) in vec4 fragSpotLightWorldSpace[MAX_SPOT_LIGHTS];
/////////////////////////////////////////////////////////////////////////////////////
// FRAGMENT INPUT
/////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////
// FRAGMENT OUTPUT
/////////////////////////////////////////////////////////////////////////////////////
layout (location = 0) out vec4 outColor;
/////////////////////////////////////////////////////////////////////////////////////
// FRAGMENT OUTPUT
/////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////
// DESCRIPTOR SET 0 : GLOBAL
/////////////////////////////////////////////////////////////////////////////////////
struct PointLight
{
	vec4 position;     // position x,y,z
	vec4 color;        // color r=x, g=y, b=z, a=intensity
};

struct SpotLight
{
	vec4 position;     // position x,y,z
	vec4 color;        // color r=x, g=y, b=z, a=intensity
	vec4 direction;    // direction x, y, z
	vec4 cutOffs;      // CutOffs x=innerCutoff y=outerCutoff

};

struct DirectionalLight
{
	vec4 direction;    // direction x, y, z, w=ambientStrength
	vec4 color;        // color r=x, g=y, b=z, a=intensity
};

struct EditorCameraData
{
	mat4 projectionMatrix;
	mat4 viewMatrix;
	mat4 inverseViewMatrix;
};

layout(set = 0, binding = 0) uniform GlobalUbo
{
	EditorCameraData cameraData;

	DirectionalLight directionalLightData;

	PointLight pointLights[MAX_POINT_LIGHTS];
	SpotLight spotLights[MAX_SPOT_LIGHTS];
	int numOfActivePointLights;
	int numOfActiveSpotLights;
}globalUbo;

layout(set = 0, binding = 1) uniform sampler2D DefaultTexture;
/////////////////////////////////////////////////////////////////////////////////////
// DESCRIPTOR SET 0 : GLOBAL
/////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////
// DESCRIPTOR SET 1 : DIRECTIONAL LIGHT PROJECTION FOR CASCADED SHADOW MAP
/////////////////////////////////////////////////////////////////////////////////////
layout(set = 1, binding = 0) uniform CascadedShadowPassUBO
{
	mat4 lightProjection[CASCADE_SHADOW_MAP_COUNT];
    vec4 cascadeSplits;
}cascadedShadowPassUBO;
/////////////////////////////////////////////////////////////////////////////////////
// DESCRIPTOR SET 1 : DIRECTIONAL LIGHT PROJECTION FOR CASCADED SHADOW MAP
/////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////
// DESCRIPTOR SET 2 : DIRECTIONAL LIGHT SHADOW MAP
/////////////////////////////////////////////////////////////////////////////////////
layout(set = 2, binding = 0) uniform sampler2DArray cascadedShadowMap;
/////////////////////////////////////////////////////////////////////////////////////
// DESCRIPTOR SET 2 : DIRECTIONAL LIGHT SHADOW MAP
/////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////
// DESCRIPTOR SET 3 : POINT LIGHT SHADOW CUBEMAP ARRAY
/////////////////////////////////////////////////////////////////////////////////////
layout (set = 3, binding = 0) uniform samplerCubeArray pointShadowCubeMap;
/////////////////////////////////////////////////////////////////////////////////////
// DESCRIPTOR SET 3 : POINT LIGHT SHADOW CUBEMAP ARRAY
/////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////
// DESCRIPTOR SET 4 : SPOT LIGHT SHADOW CUBEMAP ARRAY
/////////////////////////////////////////////////////////////////////////////////////
layout(set = 4, binding = 0) uniform sampler2DArray spotShadowMap;
/////////////////////////////////////////////////////////////////////////////////////
// DESCRIPTOR SET 4 : SPOT LIGHT SHADOW CUBEMAP ARRAY
/////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////
// DESCRIPTOR SET 5 : MATERIAL TEXTURES
/////////////////////////////////////////////////////////////////////////////////////
layout(set = 5, binding = 0) uniform sampler2D albedoTexture;
layout(set = 5, binding = 1) uniform sampler2D normalTexture;
layout(set = 5, binding = 2) uniform sampler2D metallicRoughnessTexture;
/////////////////////////////////////////////////////////////////////////////////////
// DESCRIPTOR SET 5 : MATERIAL TEXTURES
/////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////
// PUSH CONSTANTS : MAIN
/////////////////////////////////////////////////////////////////////////////////////
layout(push_constant) uniform Push 
{
	mat4 modelMatrix;
	mat4 normalMatrix;
}push;
/////////////////////////////////////////////////////////////////////////////////////
// PUSH CONSTANTS : MAIN
/////////////////////////////////////////////////////////////////////////////////////






/////////////////////////////////////////////////////////////////////////////////////
// HELPER FUNCTIONS
/////////////////////////////////////////////////////////////////////////////////////
vec3 getNormalFromMap()
{
    vec3 tangentNormal = texture(normalTexture, fragUV).xyz * 2.0 - 1.0;

    vec3 Q1  = dFdx(fragModelWorldSpace);
    vec3 Q2  = dFdy(fragModelWorldSpace);
    vec2 st1 = dFdx(fragUV);
    vec2 st2 = dFdy(fragUV);

    vec3 N   = normalize(fragNormalWorldSpace);
    vec3 T  = normalize(Q1*st2.t - Q2*st1.t);
    vec3 B  = -normalize(cross(N, T));
    mat3 TBN = mat3(T, B, N);

    return normalize(TBN * tangentNormal);
}




// Approximate the ratio between how much the surface reflects and how much it refracts.
// The F0 parameter is the surface reflection at zero incidence or how much the surface reflects
// if looking directly at the surface.
vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// Approximate the relative surface area of microfacets exactly aligned to H.
// Using Trowbridge-Reitz GGX.
float distributionGGX(vec3 N, vec3 H, float roughness) {
    // Based on observations by Disney and adopted by Epic Games, the lighting looks more correct
    // squaring the roughness in both the geometry and normal distribution function.
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float num = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return num / denom;
}

// Approximate the relative surface area where micro-facet details occlude light.
float geometrySchlickGGX(float NdotV, float roughness) {
    // NOTE: Roughness needs to be remapped depending on whether we are using direct lighting or IBL.
    float r = (roughness + 1.0);
    float kDirect = (r * r) / 8.0;
    float k = kDirect;

    float num = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return num / denom;
}

// Smith's method: Take into account view direction (obstruction) and light direction (shadowing).
float geometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = geometrySchlickGGX(NdotV, roughness);
    float ggx1 = geometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

vec4 srgb_to_linear(vec4 srgb) {
    vec3 color_srgb = srgb.rgb;
    vec3 selector = clamp(ceil(color_srgb - 0.04045), 0.0, 1.0); // 0 if under value, 1 if over
    vec3 under = color_srgb / 12.92;
    vec3 over = pow((color_srgb + 0.055) / 1.055, vec3(2.4));
    vec3 result = mix(under, over, selector);
    return vec4(result, srgb.a);
}

vec3 PointLightCalculation(vec3 albedoValue, float metallicValue, float roughnessValue, vec3 viewToFragPos, vec3 normalFromMap, PointLight light, float lightCount)
{
    // Light direction.
    vec3 L = light.position.xyz - fragModelWorldSpace;
    float lightToPixelDist = length(L);
    L = normalize(L);
    
    // Attenuate light by the inverse square law.
    float attenuation = 1.0 / (lightToPixelDist * lightToPixelDist);
    vec3 radiance = light.color.rgb * light.color.w * attenuation;
    
    vec3 H = normalize(viewToFragPos + L);
    
    
    
    // Compute the BRDF term using the Cook-Torrance BRDF
    // Fresnel (F)
    // Dielectric materials are assumed to have a constant F0 value of 0.04.
    vec3 F0 = vec3(0.04);
    // Metal will tint the base reflectivity by the surface's color.
    F0 = mix(F0, albedoValue, metallicValue);
    vec3 F = fresnelSchlick(max(dot(H, viewToFragPos), 0.0), F0);
    
    // Normal distribution function (D)
    float NDF = distributionGGX(normalFromMap, H, roughnessValue);
    // Geometry (G)
    float G = geometrySmith(normalFromMap, viewToFragPos, L, roughnessValue);
    
    // Cook-Torrance BRDF
    vec3 numerator = NDF * G * F;
    float denominator = 4.0 * max(dot(normalFromMap, viewToFragPos), 0.0) * max(dot(normalFromMap, L), 0.0) + 0.0001; // Prevent divide by zero.
    vec3 specular = numerator / denominator;
    
    // Specular ratio.
    vec3 kS = F;
    // Diffuse ratio.
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - metallicValue; // Metallic surfaces don't refract light, so we nullify the diffuse term.
    
    // Calculate the light's contribution to the reflectance equation.
    float NdotL = max(dot(normalFromMap, L), 0.0);
    vec3 Lo = (kD * albedoValue / PI + specular) * radiance * NdotL;

    //shadow
    vec3 fragToLight =  fragModelWorldSpace - light.position.xyz;
    float currentDepth = length(fragToLight);

    float shadow = 0.0;
    {
         float bias = -0.00005f;
        int samples = 20;
        float viewDistance = length(globalUbo.cameraData.inverseViewMatrix[3].xyz - fragModelWorldSpace);
        float diskRadius = (1.0 + (viewDistance / 25.0f)) / 25.0;
        for(int i = 0; i < samples; ++i)
        {
            vec4 loc =  vec4(fragToLight + ((gridSamplingDisk[i])/10 * diskRadius), lightCount); 
            float closestDepth = texture(pointShadowCubeMap, loc).r;
            //closestDepth *= 100.0f;   // undo mapping [0;1]
            if(currentDepth - bias > closestDepth)
               shadow += 1.0;
        }

        shadow /= float(samples); 
    }

    return Lo * (1.0f - shadow);
}

vec3 DirectionalLightCalculation(vec3 albedoValue, float metallicValue, float roughnessValue, vec3 viewToFragPos, vec3 normalFromMap, vec3 lightDirection, vec4 lightColor, vec3 ambient)
{
    vec3 L = normalize(-lightDirection);

    float NdotL = max(0.0f, dot(normalFromMap, L));
    float NdotV = max(0.0f, dot(normalFromMap, viewToFragPos));

    vec3 H = normalize(viewToFragPos + L);
    float NdotH = max(0.0f, dot(normalFromMap, H));

    // Compute the BRDF term using the Cook-Torrance BRDF
    // Fresnel (F)
    // Dielectric materials are assumed to have a constant F0 value of 0.04.
    vec3 F0 = vec3(0.04);
    // Metal will tint the base reflectivity by the surface's color.
    F0 = mix(F0, albedoValue, metallicValue);
    vec3 F = fresnelSchlick(max(dot(H, viewToFragPos), 0.0), F0);
    float D = distributionGGX(normalFromMap, H, roughnessValue);
    float G = geometrySmith(normalFromMap, viewToFragPos, L, roughnessValue);

    vec3 kd = (1.0f - F) * (1.0f - metallicValue);
    vec3 diffuse = kd * albedoValue;

    vec3 nominator = F * G * D;
    float denominator = max(epsilon, 4.0f * NdotV * NdotL);
    vec3 specular = nominator / denominator;
    specular = clamp(specular, vec3(0.0f), vec3(10.0f));

    vec3 result = (diffuse + specular) * (lightColor.xyz * lightColor.w) * NdotL;


    //shadow calculation
	float shadow = 0.0f;
    //cascaded shadow calculation
    	// Get cascade index for the current fragment's view position
	uint cascadeIndex = 0;
	for(uint i = 0; i < CASCADE_SHADOW_MAP_COUNT - 1; ++i) {
		if(-fragViewPos.z < cascadedShadowPassUBO.cascadeSplits[i]) {	
			cascadeIndex = i + 1;
		}
	}

    vec4 shadowCoord = (biasMat * cascadedShadowPassUBO.lightProjection[cascadeIndex] * vec4(fragModelWorldSpace, 1.0f));
    shadowCoord = shadowCoord / shadowCoord.w;
     if(shadowCoord.z > -1.0f && shadowCoord.z < 1.0)
	{
		float currentDepth = shadowCoord.z;
        
        float bias = 0.000001f; // Bias value

		int sampleRadius = 3;
		vec3 pixelSize =  1.0 / textureSize(cascadedShadowMap, 0);

		for(int y = -sampleRadius; y <= sampleRadius; y++)
		{
			for(int x = -sampleRadius; x <= sampleRadius; x++)
			{
				float closestDepth = texture(cascadedShadowMap, vec3(shadowCoord.xy + vec2(x, y) * pixelSize.xy/10, cascadeIndex)).r;
				if ( currentDepth - bias > closestDepth) // included bias check
                    shadow += 1.0f;
			}    
		}
		shadow /= pow((sampleRadius * 2 + 1), 2);
	}
//    debug cascade shadows
//    switch(cascadeIndex) {
//			case 0 : 
//				result.rgb *= vec3(1.0f, 0.25f, 0.25f);
//				break;
//			case 1 : 
//				result.rgb *= vec3(0.25f, 1.0f, 0.25f);
//				break;
//			case 2 : 
//				result.rgb *= vec3(0.25f, 0.25f, 1.0f);
//				break;
//			case 3 : 
//				result.rgb *= vec3(1.0f, 1.0f, 0.25f);
//				break;
//		}

    return result * (1.0f - shadow);
}


vec3 SpotLightCalculation(vec3 albedoValue, float metallicValue, float roughnessValue, vec3 viewToFragPos, vec3 normalFromMap, SpotLight light, float lightIndex)
{
    // Light direction.
    vec3 L = light.position.xyz - fragModelWorldSpace;
    float lightToPixelDist = length(L);
    L = normalize(L);
    
    // Attenuate light by the inverse square law.
    float attenuation = 1.0 / (lightToPixelDist * lightToPixelDist);
    vec3 radiance = light.color.rgb * light.color.w * attenuation;
    
    vec3 H = normalize(viewToFragPos + L);
    
    // Compute the BRDF term using the Cook-Torrance BRDF
    // Fresnel (F)
    // Dielectric materials are assumed to have a constant F0 value of 0.04.
    vec3 F0 = vec3(0.04);
    // Metal will tint the base reflectivity by the surface's color.
    F0 = mix(F0, albedoValue, metallicValue);
    vec3 F = fresnelSchlick(max(dot(H, viewToFragPos), 0.0), F0);
    
    // Normal distribution function (D)
    float NDF = distributionGGX(normalFromMap, H, roughnessValue);
    // Geometry (G)
    float G = geometrySmith(normalFromMap, viewToFragPos, L, roughnessValue);
    
    // Cook-Torrance BRDF
    vec3 numerator = NDF * G * F;
    float denominator = 4.0 * max(dot(normalFromMap, viewToFragPos), 0.0) * max(dot(normalFromMap, L), 0.0) + 0.0001; // Prevent divide by zero.
    vec3 specular = numerator / denominator;
    
    // Specular ratio.
    vec3 kS = F;
    // Diffuse ratio.
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - metallicValue; // Metallic surfaces don't refract light, so we nullify the diffuse term.
    
    // spotlight (soft edge)
    float theta = dot(L, normalize(-light.direction.xyz)); 
    float ep = (light.cutOffs.x - light.cutOffs.y); //postion w is cutoff, direction w is outer cutoff
    float intensity = clamp((theta - light.cutOffs.y) / ep, 0.0, 1.0);
    kD  *= intensity;
    specular *= intensity;
    
    // Calculate the light's contribution to the reflectance equation.
    float NdotL = max(dot(normalFromMap, L), 0.0);
    vec3 Lo = (kD * albedoValue / PI + specular) * radiance * NdotL;

    //shadow calculation
    float shadow = 0.0f;
    // Sets lightCoords to cull space
	vec4 lightCoords = fragSpotLightWorldSpace[int(lightIndex)]/fragSpotLightWorldSpace[int(lightIndex)].w;
	//if(lightCoords.z > 0.0 && lightCoords.z < 1.0 && lightCoords.x > 0.0 && lightCoords.x < 1.0 && lightCoords.y > 0.0 && lightCoords.y < 1.0) // included x and y coord

	if(lightCoords.z > -1.0f && lightCoords.z < 1.0)
	//if(currentDepth > -1.0f && currentDepth < 1.0)
    {
		//float currentDepth = lightCoords.z;
        vec3 fragToLight =  fragModelWorldSpace - light.position.xyz;
        float currentDepth = length(fragToLight);
        
        float bias = 0.00005f; // Bias value

		int sampleRadius = 20;
        vec3 pixelSize = 1.0 / textureSize(spotShadowMap, 0);

		for(int y = -sampleRadius; y <= sampleRadius; y++)
		{
			for(int x = -sampleRadius; x <= sampleRadius; x++)
			{
                vec2 temp = lightCoords.xy + vec2(x, y) * pixelSize.xy/10;
				float closestDepth = texture(spotShadowMap, vec3(temp.st, lightIndex)).r;
				if (currentDepth - bias > closestDepth) // included bias check
                    shadow += 1.0f;
			}    
		}
		shadow /= pow((sampleRadius * 2 + 1), 2);
	}
    
    return Lo * (1.0f - shadow);
}
/////////////////////////////////////////////////////////////////////////////////////
// HELPER FUNCTIONS
/////////////////////////////////////////////////////////////////////////////////////

void main()
{
    vec3 albedo = texture(albedoTexture, fragUV).rgb;
    float metallic = texture(metallicRoughnessTexture, fragUV).b;
    float roughness = texture(metallicRoughnessTexture, fragUV).g;
    float ao = 1.0f;

    vec3 cameraPosWorldSpace = globalUbo.cameraData.inverseViewMatrix[3].xyz;

    vec3 N = normalize(fragNormalWorldSpace); // Surface normal
    vec3 V = normalize(cameraPosWorldSpace - fragModelWorldSpace); // View direction

    // Total reflected radiance back to the viewer.
    vec3 Lo = vec3(0.0);

    // Point Light List
    for(float i = 0; i < globalUbo.numOfActivePointLights; i++)
    {
        PointLight light = globalUbo.pointLights[int(i)];
        Lo += PointLightCalculation(albedo, metallic, roughness, V, N, light, i);
    }

    // Spot Light List
    for(float j = 0; j < globalUbo.numOfActiveSpotLights; j++)
    {
        SpotLight light = globalUbo.spotLights[int(j)];
        Lo += SpotLightCalculation(albedo, metallic, roughness, V, N, light, j);
    }

    // Improvised ambient term.
    vec3 ambient = globalUbo.directionalLightData.direction.w * albedo * ao;
    vec3 dirLi = DirectionalLightCalculation(albedo, metallic, roughness, V, N, globalUbo.directionalLightData.direction.xyz, globalUbo.directionalLightData.color, ambient);

    vec3 color = ambient + Lo + dirLi;
    // Tone mapping and gamma correction.
    color = color / (color + vec3(1.0));
    
    outColor = vec4(color, 1.0);
}
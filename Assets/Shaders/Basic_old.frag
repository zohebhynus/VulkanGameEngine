#version 450

struct PointLight
{
	vec4 position; // ignore w
	vec4 color; // w is intensity
};

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec3 fragModelWorldSpace;
layout(location = 2) in vec3 fragNormalWorldSpace;
layout(location = 3) in vec3 fragTangent;
layout(location = 4) in vec2 fragUV;

layout (location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform GlobalUbo
{
	mat4 projectionMatrix;
	mat4 viewMatrix;
	mat4 inverseViewMatrix;
	vec4 ambientColor;
	PointLight pointLights[10];
	int numActiveLights;
}ubo;
layout(set = 0, binding = 1) uniform sampler2D DefaultTexture;

layout(set = 1, binding = 0) uniform sampler2D albedoTexture;
layout(set = 1, binding = 1) uniform sampler2D normalTexture;
layout(set = 1, binding = 2) uniform sampler2D metallicRoughnessTexture;


layout(push_constant) uniform Push 
{
	mat4 modelMatrix;
	mat4 normalMatrix;
}push;




const float PI = 3.14159265359;
// ----------------------------------------------------------------------------
// Easy trick to get tangent-normals to world-space to keep PBR code simplified.
// Don't worry if you don't get what's going on; you generally want to do normal 
// mapping the usual way for performance anyways; I do plan make a note of this 
// technique somewhere later in the normal mapping tutorial.
vec3 getNormalFromMap()
{
    vec3 normal = texture(normalTexture, fragUV).xyz * 2.0 - 1.0;

    vec3 Q1  = dFdx(fragModelWorldSpace);
    vec3 Q2  = dFdy(fragModelWorldSpace);
    vec2 st1 = dFdx(fragUV);
    vec2 st2 = dFdy(fragUV);

    vec3 N   = normalize(fragNormalWorldSpace);
    vec3 T  = normalize(Q1*st2.t - Q2*st1.t);
    vec3 B  = -normalize(cross(N, T));
    mat3 TBN = mat3(T, B, N);

    return normalize(TBN * normal);
}
// ----------------------------------------------------------------------------
float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a = roughness*roughness;
    float a2 = a*a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;

    float nom   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return nom / denom;
}
// ----------------------------------------------------------------------------
float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;

    float nom   = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return nom / denom;
}
// ----------------------------------------------------------------------------
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}
// ----------------------------------------------------------------------------
vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}
// ----------------------------------------------------------------------------



void main()
{
	vec3 diffuseLight = ubo.ambientColor.xyz * ubo.ambientColor.w;
	vec3 specularLight = vec3(0.0f);

	vec3 albedo = texture(albedoTexture, fragUV).rgb;
	float metallic = texture(albedoTexture, fragUV).b;
	float roughness = texture(albedoTexture, fragUV).g;
	vec3 N = normalize(fragNormalWorldSpace);

	vec3 cameraPosWorldSpace = ubo.inverseViewMatrix[3].xyz;
	vec3 viewDirection = normalize(cameraPosWorldSpace - fragModelWorldSpace);

	// calculate reflectance at normal incidence; if dia-electric (like plastic) use F0 
    // of 0.04 and if it's a metal, use the albedo color as F0 (metallic workflow)    
    vec3 F0 = vec3(0.04); 
    F0 = mix(F0, albedo, metallic);

    // reflectance equation
    vec3 Lo = vec3(0.0);
	for(int i = 0;  i < ubo.numActiveLights; i++)
	{
		PointLight light = ubo.pointLights[i];
		vec3 L = normalize(light.position.xyz - fragModelWorldSpace);
		vec3 H = normalize(viewDirection + L);
		vec3 dist = light.position.xyz - fragModelWorldSpace;
		float att = 1.0f / dot(dist, dist);
		vec3 radiance = light.color.xyz * att;


		        // Cook-Torrance BRDF
        float NDF = DistributionGGX(N, H, roughness);   
        float G   = GeometrySmith(N, viewDirection, L, roughness);      
        vec3 F    = fresnelSchlick(max(dot(H, viewDirection), 0.0), F0);

		vec3 numerator    = NDF * G * F; 
        float denominator = 4.0 * max(dot(N, viewDirection), 0.0) * max(dot(N, L), 0.0) + 0.0001; // + 0.0001 to prevent divide by zero
        vec3 specular = numerator / denominator;

		        // kS is equal to Fresnel
        vec3 kS = F;
        // for energy conservation, the diffuse and specular light can't
        // be above 1.0 (unless the surface emits light); to preserve this
        // relationship the diffuse component (kD) should equal 1.0 - kS.
        vec3 kD = vec3(1.0) - kS;
        // multiply kD by the inverse metalness such that only non-metals 
        // have diffuse lighting, or a linear blend if partly metal (pure metals
        // have no diffuse light).
        kD *= 1.0 - metallic;	  

        // scale light by NdotL
        float NdotL = max(dot(N, L), 0.0); 

        // add to outgoing radiance Lo
        Lo += (kD * albedo / PI + specular) * radiance * NdotL;  // note that we already multiplied the BRDF by the Fresnel (kS) so we won't multiply by kS again
	}
    vec3 ambient =  albedo * ubo.ambientColor.w;

	vec3 imageColor = ambient + Lo;

	outColor = vec4(imageColor, 1.0f);
}










//void main()
//{
//	vec3 diffuseLight = ubo.ambientColor.xyz * ubo.ambientColor.w;
//	vec3 specularLight = vec3(0.0f);
//	vec3 surfaceNormal = normalize(fragNormalWorldSpace);
//
//	vec3 albedo = texture(albedoTexture, fragUV).rgb;
//	float metallic = texture(albedoTexture, fragUV).r;
//	float roughness = texture(albedoTexture, fragUV).g;
//
//	vec3 cameraPosWorldSpace = ubo.inverseViewMatrix[3].xyz;
//	vec3 viewDirection = normalize(cameraPosWorldSpace - fragModelWorldSpace);
//
//	for(int i = 0;  i < ubo.numActiveLights; i++)
//	{
//		PointLight light = ubo.pointLights[i];
//		vec3 directionToLight = light.position.xyz - fragModelWorldSpace;
//		float att = 1.0f / dot(directionToLight, directionToLight);
//		directionToLight = normalize(directionToLight);
//
//		float cosAngIncidence = max(dot(surfaceNormal, directionToLight), 0.0f);
//		vec3 intensity = light.color.xyz * light.color.w * att;
//		diffuseLight += intensity * cosAngIncidence;
//
//		//specular
//		vec3 halfAngle = normalize(directionToLight + viewDirection);
//		float blinnTerm = dot(surfaceNormal, halfAngle);
//		blinnTerm = clamp(blinnTerm, 0, 1);
//		blinnTerm = pow(blinnTerm, 512.0f); // higher is sharper highlight
//		specularLight += intensity * blinnTerm;
//	}
//
//	vec3 imageColor = texture(albedoTexture, fragUV).rgb;
//	outColor = vec4(diffuseLight * imageColor + specularLight * fragColor, 1.0f);
//}
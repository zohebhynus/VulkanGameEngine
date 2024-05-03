#version 450 core

/////////////////////////////////////////////////////////////////////////////////////
// VERTEX INPUT
/////////////////////////////////////////////////////////////////////////////////////
layout(location = 0) in vec3 position;
layout(location = 1) in vec3 color;
layout(location = 2) in vec3 normal;
layout(location = 3) in vec3 tangent;
layout(location = 4) in vec2 uv;
/////////////////////////////////////////////////////////////////////////////////////
// VERTEX INPUT
/////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////
// DESCRIPTOR SET 1 : DIRECTIONAL LIGHT PROJECTIONS FOR SHADOW MAP
/////////////////////////////////////////////////////////////////////////////////////
layout(set = 1, binding = 0) uniform ShadowPassUBO
{
	mat4 lightProjection;
}shadowPassUBO;
/////////////////////////////////////////////////////////////////////////////////////
// DESCRIPTOR SET 1 : DIRECTIONAL LIGHT PROJECTIONS FOR SHADOW MAP
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

void main()
{
	gl_Position = shadowPassUBO.lightProjection * push.modelMatrix * vec4(position, 1.0f);
}
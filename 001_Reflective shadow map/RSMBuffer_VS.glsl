#version 430 core
layout(location = 0) in vec3 _Position;
layout(location = 1) in vec3 _Normal;
layout(location = 2) in vec2 _TexCoord;

layout(std140, binding = 0) uniform u_Matrices4ProjectionWorld
{
	mat4 u_ProjectionMatrix;
	mat4 u_ViewMatrix;
};

uniform mat4 u_ModelMatrix;
uniform mat4 u_LightVPMatrix;

out vec2 v2f_TexCoords;
out vec3 v2f_Normal;
out vec3 v2f_FragPosInViewSpace;
out vec3 v2f_FragposInLightSpace;

void main()
{
	vec4 FragPosInWorldSpace = u_ModelMatrix * vec4(_Position, 1.0f);
	vec4 FragPosInLightSpace = u_LightVPMatrix * FragPosInWorldSpace;
	// 存储光源坐标下位置
	v2f_FragposInLightSpace = FragPosInLightSpace.xyz;
	gl_Position = FragPosInLightSpace;
	v2f_TexCoords = _TexCoord;
	//存储的是在相机空间下的位置以及法线，不是光源空间下的
	v2f_Normal = normalize(mat3(transpose(inverse(u_ViewMatrix * u_ModelMatrix))) * _Normal);	//这个可以在外面算好了传进来
	v2f_FragPosInViewSpace = vec3(u_ViewMatrix * FragPosInWorldSpace);
	
}
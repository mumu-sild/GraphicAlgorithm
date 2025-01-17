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

out vec2 v2f_TexCoords;
out vec3 v2f_ViewSpaceNormal;
out vec3 v2f_FragPosInViewSpace;
out vec3 v2f_WorldSpaceNormal;

void main()
{

	vec4 FragPosInViewSpace = u_ViewMatrix * u_ModelMatrix * vec4(_Position, 1.0f);
	gl_Position = u_ProjectionMatrix * FragPosInViewSpace;
	v2f_TexCoords = _TexCoord;
	// 视口矩阵下的normal？？？？ // 删掉了 u_ViewMatrix * 
	v2f_WorldSpaceNormal = normalize(mat3(transpose(inverse(u_ModelMatrix))) * _Normal);
	// 不能直接在转置后的v2f_WorldSpaceNormal上乘以u_ViewMatrix！！！WHY？
	v2f_ViewSpaceNormal =  normalize(mat3(transpose(inverse(u_ViewMatrix * u_ModelMatrix))) * _Normal);
	v2f_FragPosInViewSpace = vec3(FragPosInViewSpace);
	
}
#version 430 core

in  vec3 v2f_TexCoords;

layout(location = 0) out vec4 FragColor;
uniform samplerCube u_Skybox;

void main()
{
	FragColor = texture(u_Skybox, v2f_TexCoords);
}
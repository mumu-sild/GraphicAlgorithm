#version 430 core
#pragma optionNV (unroll all)	//暂时不知道有没有起作用

#define LOCAL_GROUP_SIZE 32
#define VPL_NUM 32

layout (local_size_x = LOCAL_GROUP_SIZE, local_size_y = LOCAL_GROUP_SIZE) in;

layout (rgba32f, binding = 0) uniform writeonly image2D u_OutputImage;

layout(std140, binding = 0) uniform u_Matrices4ProjectionWorld
{
	mat4 u_ProjectionMatrix;
	mat4 u_ViewMatrix;
};

layout(std140, binding = 1) uniform VPLsSampleCoordsAndWeights
{
	vec4 u_VPLsSampleCoordsAndWeights[VPL_NUM];
};

uniform sampler2D u_AlbedoTexture;
uniform sampler2D u_NormalTexture;
uniform sampler2D u_PositionTexture;
uniform sampler2D u_RSMFluxTexture;
uniform sampler2D u_RSMNormalTexture;		
uniform sampler2D u_RSMPositionTexture;		
uniform sampler2D u_RSMLightPositionTexture;
uniform mat4  u_LightVPMatrixMulInverseCameraViewMatrix;
uniform float u_MaxSampleRadius;
uniform int   u_RSMSize;
uniform int   u_VPLNum;
uniform vec3  u_LightDirInViewSpace;

vec3 calcVPLIrradiance(vec3 vVPLFlux, vec3 vVPLNormal, vec3 vVPLPos, vec3 vFragPos, vec3 vFragNormal, float vWeight)
{
	vec3 VPL2Frag = normalize(vFragPos - vVPLPos);
	return vVPLFlux * max(dot(vVPLNormal, VPL2Frag), 0) * max(dot(vFragNormal, -VPL2Frag), 0) * vWeight;
}

void main()
{
	if(u_VPLNum != VPL_NUM)
		return;

	ivec2 FragPos = ivec2(gl_GlobalInvocationID.xy);//当前执行单元在全局工作组中的位置的有效索引
	vec3 FragViewNormal = normalize(texelFetch(u_NormalTexture, FragPos, 0).xyz);
	vec3 FragAlbedo = texelFetch(u_AlbedoTexture, FragPos, 0).xyz;
	vec3 FragViewPos = texelFetch(u_PositionTexture, FragPos, 0).xyz;

	// 获取 摄像机视口下像素 在光源视口下的位置坐标
	vec4 FragPosInLightSpace = u_LightVPMatrixMulInverseCameraViewMatrix * vec4(FragViewPos, 1);
	FragPosInLightSpace /= FragPosInLightSpace.w;
	vec2 FragNDCPos4Light = (FragPosInLightSpace.xy + 1) / 2;

	// 直接光照&环境光照
	vec3 testOutput;
	vec3 DirectIllumination;
	// 如果该像素在RSM范围之外，以0.1倍源像素作为其环境光照
	// FragPosInLightSpace.z范围在【-1，0】的区间中.属于可被光照直接照射的范围
	// 这里可通过与RSM中的深度进行比较，来确定是否有阴影生成，或是否可被作为次级光源
	if(	FragPosInLightSpace.z < -1.0f || FragPosInLightSpace.z > 0.0f||\
		FragPosInLightSpace.x >=  1.0f || FragPosInLightSpace.x <= -1.0f||\
		FragPosInLightSpace.y >=  1.0f || FragPosInLightSpace.y <= -1.0f )//
	{
		DirectIllumination = vec3(0.1) * FragAlbedo;
		testOutput = vec3(0.f,0.f,0.f);
	}
	else//反之，将原像素经过夹角衰减后的值作为直接光照
	{	
		// view space下的光照坐标
		vec3 RSWLightPosition =texture(u_RSMLightPositionTexture,FragNDCPos4Light.xy).xyz;
		
		// 判断是否在阴影中(近小【-1】远大【0】)
		if(RSWLightPosition.z + 0.001 > FragPosInLightSpace.z){//不在阴影中
			DirectIllumination = FragAlbedo* max(dot(-u_LightDirInViewSpace, FragViewNormal), 0.1);// ;
		}
		else{//在阴影中
		   DirectIllumination = vec3(0.1) * FragAlbedo;
		}
		
		
		//if(FragPosInLightSpace.z>=-1&&FragPosInLightSpace.z<=0)
		//testOutput = (1+vec3(RSWLightPosition.z,RSWLightPosition.z,RSWLightPosition.z))/2;
		//testOutput = vec3(-RSWViewPosInLightSpace.z,-RSWViewPosInLightSpace.z,-RSWViewPosInLightSpace.z);

	}
	// 将结果保存在ShadingTexture
	//imageStore(u_OutputImage, FragPos, vec4(testOutput,0));

	//直接光可照射的部位【没有做阴影处理】
	//imageStore(u_OutputImage, FragPos, vec4(DirectIllumination, 1.0));
	//return;

	// 计算间接光照
	vec3 IndirectIllumination = vec3(0);
	float RSMTexelSize = 1.0 / u_RSMSize;
	for(int i = 0; i < u_VPLNum; ++i)
	{
		if(FragPosInLightSpace.z > -1.0f && FragPosInLightSpace.z <0.0f){
			//获取随机采样数组
			vec3 VPLSampleCoordAndWeight = u_VPLsSampleCoordsAndWeights[i].xyz;
			// 在光源视口下的该像素点周围一圈进行采样
			vec2 VPLSamplePos = FragNDCPos4Light + u_MaxSampleRadius * VPLSampleCoordAndWeight.xy * RSMTexelSize;
			// 在RSM图中获取采样点的次级光源颜色值
			vec3 VPLFlux = texture(u_RSMFluxTexture, VPLSamplePos).xyz;
			// 获取采样点在摄像机视口坐标下的的法线和位置坐标
			vec3 VPLNormalInViewSpace = normalize(texture(u_RSMNormalTexture, VPLSamplePos).xyz);
			vec3 VPLPositionInViewSpace = texture(u_RSMPositionTexture, VPLSamplePos).xyz;
			// 计算该采样点对该像素的间接光照值
			IndirectIllumination += calcVPLIrradiance(VPLFlux, VPLNormalInViewSpace, VPLPositionInViewSpace, FragViewPos, FragViewNormal, VPLSampleCoordAndWeight.z);
		}
	}
	IndirectIllumination *= FragAlbedo;

	//间接光
	vec3 Result = IndirectIllumination / u_VPLNum;
	//直接光
	//vec3 Result = DirectIllumination;
	//vec3 Result = DirectIllumination  + IndirectIllumination / u_VPLNum ;//增大间接光10倍 * 10

	// 将结果保存在ShadingTexture
	imageStore(u_OutputImage, FragPos, vec4(Result, 1.0));
}
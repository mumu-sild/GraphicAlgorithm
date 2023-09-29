#version 430 core
#pragma optionNV (unroll all)	//��ʱ��֪����û��������

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

	ivec2 FragPos = ivec2(gl_GlobalInvocationID.xy);//��ǰִ�е�Ԫ��ȫ�ֹ������е�λ�õ���Ч����
	vec3 FragViewNormal = normalize(texelFetch(u_NormalTexture, FragPos, 0).xyz);
	vec3 FragAlbedo = texelFetch(u_AlbedoTexture, FragPos, 0).xyz;
	vec3 FragViewPos = texelFetch(u_PositionTexture, FragPos, 0).xyz;

	// ��ȡ ������ӿ������� �ڹ�Դ�ӿ��µ�λ������
	vec4 FragPosInLightSpace = u_LightVPMatrixMulInverseCameraViewMatrix * vec4(FragViewPos, 1);
	FragPosInLightSpace /= FragPosInLightSpace.w;
	vec2 FragNDCPos4Light = (FragPosInLightSpace.xy + 1) / 2;

	// ֱ�ӹ���&��������
	vec3 testOutput;
	vec3 DirectIllumination;
	// �����������RSM��Χ֮�⣬��0.1��Դ������Ϊ�价������
	// FragPosInLightSpace.z��Χ�ڡ�-1��0����������.���ڿɱ�����ֱ������ķ�Χ
	// �����ͨ����RSM�е���Ƚ��бȽϣ���ȷ���Ƿ�����Ӱ���ɣ����Ƿ�ɱ���Ϊ�μ���Դ
	if(	FragPosInLightSpace.z < -1.0f || FragPosInLightSpace.z > 0.0f||\
		FragPosInLightSpace.x >=  1.0f || FragPosInLightSpace.x <= -1.0f||\
		FragPosInLightSpace.y >=  1.0f || FragPosInLightSpace.y <= -1.0f )//
	{
		DirectIllumination = vec3(0.1) * FragAlbedo;
		testOutput = vec3(0.f,0.f,0.f);
	}
	else//��֮����ԭ���ؾ����н�˥�����ֵ��Ϊֱ�ӹ���
	{	
		// view space�µĹ�������
		vec3 RSWLightPosition =texture(u_RSMLightPositionTexture,FragNDCPos4Light.xy).xyz;
		
		// �ж��Ƿ�����Ӱ��(��С��-1��Զ��0��)
		if(RSWLightPosition.z + 0.001 > FragPosInLightSpace.z){//������Ӱ��
			DirectIllumination = FragAlbedo* max(dot(-u_LightDirInViewSpace, FragViewNormal), 0.1);// ;
		}
		else{//����Ӱ��
		   DirectIllumination = vec3(0.1) * FragAlbedo;
		}
		
		
		//if(FragPosInLightSpace.z>=-1&&FragPosInLightSpace.z<=0)
		//testOutput = (1+vec3(RSWLightPosition.z,RSWLightPosition.z,RSWLightPosition.z))/2;
		//testOutput = vec3(-RSWViewPosInLightSpace.z,-RSWViewPosInLightSpace.z,-RSWViewPosInLightSpace.z);

	}
	// �����������ShadingTexture
	//imageStore(u_OutputImage, FragPos, vec4(testOutput,0));

	//ֱ�ӹ������Ĳ�λ��û������Ӱ����
	//imageStore(u_OutputImage, FragPos, vec4(DirectIllumination, 1.0));
	//return;

	// �����ӹ���
	vec3 IndirectIllumination = vec3(0);
	float RSMTexelSize = 1.0 / u_RSMSize;
	for(int i = 0; i < u_VPLNum; ++i)
	{
		if(FragPosInLightSpace.z > -1.0f && FragPosInLightSpace.z <0.0f){
			//��ȡ�����������
			vec3 VPLSampleCoordAndWeight = u_VPLsSampleCoordsAndWeights[i].xyz;
			// �ڹ�Դ�ӿ��µĸ����ص���ΧһȦ���в���
			vec2 VPLSamplePos = FragNDCPos4Light + u_MaxSampleRadius * VPLSampleCoordAndWeight.xy * RSMTexelSize;
			// ��RSMͼ�л�ȡ������Ĵμ���Դ��ɫֵ
			vec3 VPLFlux = texture(u_RSMFluxTexture, VPLSamplePos).xyz;
			// ��ȡ��������������ӿ������µĵķ��ߺ�λ������
			vec3 VPLNormalInViewSpace = normalize(texture(u_RSMNormalTexture, VPLSamplePos).xyz);
			vec3 VPLPositionInViewSpace = texture(u_RSMPositionTexture, VPLSamplePos).xyz;
			// ����ò�����Ը����صļ�ӹ���ֵ
			IndirectIllumination += calcVPLIrradiance(VPLFlux, VPLNormalInViewSpace, VPLPositionInViewSpace, FragViewPos, FragViewNormal, VPLSampleCoordAndWeight.z);
		}
	}
	IndirectIllumination *= FragAlbedo;

	//��ӹ�
	vec3 Result = IndirectIllumination / u_VPLNum;
	//ֱ�ӹ�
	//vec3 Result = DirectIllumination;
	//vec3 Result = DirectIllumination  + IndirectIllumination / u_VPLNum ;//�����ӹ�10�� * 10

	// �����������ShadingTexture
	imageStore(u_OutputImage, FragPos, vec4(Result, 1.0));
}
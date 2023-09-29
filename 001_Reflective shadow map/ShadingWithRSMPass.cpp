#include "ShadingWithRSMPass.h"
#include "Common.h"
#include "Interface.h"
#include "Utils.h"
#include "Shader.h"
#include <random>
#include <GLM/gtc/type_ptr.hpp>

CShadingWithRSMPass::CShadingWithRSMPass(const std::string& vPassName, int vExcutionOrder) : IRenderPass(vPassName, vExcutionOrder)
{
}

CShadingWithRSMPass::~CShadingWithRSMPass()
{
}

void CShadingWithRSMPass::initV()
{
	// ����������Ҫ�Ĳ�����������Ϊ��Shader�����
	auto TextureConfig = std::make_shared<ElayGraphics::STexture>(); ;
	TextureConfig->InternalFormat = GL_RGBA32F;
	TextureConfig->ExternalFormat = GL_RGBA;
	TextureConfig->DataType = GL_FLOAT;
	TextureConfig->Type4MinFilter = GL_LINEAR;
	TextureConfig->Type4MagFilter = GL_LINEAR;
	TextureConfig->ImageBindUnit = 0;//����ͼƬ�󶨵�0��λ
	genTexture(TextureConfig);
	ElayGraphics::ResourceManager::registerSharedData("ShadingTexture", TextureConfig);
	ElayGraphics::Camera::setMainCameraMoveSpeed(0.5);
	// ����Բ�����������������GL_UNIFORM_BUFFER1��binding = 1����
	__initVPLsSampleCoordsAndWeights();

	// 
	m_LightVPMatrix = ElayGraphics::ResourceManager::getSharedDataByName<glm::mat4>("LightVPMatrix");
	// ����ʹ����ComputeShader����������Ⱦ���ߣ�Ҳ������Ⱦͼ�Σ�����ֱ�ӽ���Ⱦ�����Ϊ��Ⱦ��ˮ������
	m_pShader = std::make_shared<CShader>("ShadingWithRSM_CS.glsl");
	m_pShader->activeShader();
	// ���Pass1����Ⱦ������ͼ
	m_pShader->setTextureUniformValue("u_AlbedoTexture", ElayGraphics::ResourceManager::getSharedDataByName<std::shared_ptr<ElayGraphics::STexture>>("AlbedoTexture"));
	m_pShader->setTextureUniformValue("u_NormalTexture", ElayGraphics::ResourceManager::getSharedDataByName<std::shared_ptr<ElayGraphics::STexture>>("NormalTexture"));
	m_pShader->setTextureUniformValue("u_PositionTexture", ElayGraphics::ResourceManager::getSharedDataByName<std::shared_ptr<ElayGraphics::STexture>>("PositionTexture"));
	// ���Pass2����Ⱦ������ͼ
	m_pShader->setTextureUniformValue("u_RSMFluxTexture", ElayGraphics::ResourceManager::getSharedDataByName<std::shared_ptr<ElayGraphics::STexture>>("RSMFluxTexture"));
	m_pShader->setTextureUniformValue("u_RSMNormalTexture", ElayGraphics::ResourceManager::getSharedDataByName<std::shared_ptr<ElayGraphics::STexture>>("RSMNormalTexture"));
	m_pShader->setTextureUniformValue("u_RSMPositionTexture", ElayGraphics::ResourceManager::getSharedDataByName<std::shared_ptr<ElayGraphics::STexture>>("RSMPositionTexture"));
	m_pShader->setTextureUniformValue("u_RSMLightPositionTexture", ElayGraphics::ResourceManager::getSharedDataByName<std::shared_ptr<ElayGraphics::STexture>>("RSMLightPositionTexture"));
	// ���ò����뾶��������Χ����������
	m_pShader->setFloatUniformValue("u_MaxSampleRadius", m_MaxSampleRadius);
	m_pShader->setIntUniformValue("u_RSMSize", ElayGraphics::ResourceManager::getSharedDataByName<int>("RSMResolution"));
	m_pShader->setIntUniformValue("u_VPLNum", m_VPLNum);
	m_LightDir = glm::vec4(ElayGraphics::ResourceManager::getSharedDataByName<glm::vec3>("LightDir"), 0.0f);	//���Ǹ����򣬵���ά��Ҫ��0������Ϊ1������������

	std::vector<int> LocalGroupSize;
	// �� GetProgramiv �ӿڴ��� COMPUTE_WORK_GROUP_SIZE ��������õĽ����
	// ��Compute Shader��ͨ�� 
	//		layout (local_size_x = 32, local_size_y = 16, local_size_z = 1)
	// ��ָ���ĵ�ǰ�������Ĺ������С��
	m_pShader->InquireLocalGroupSize(LocalGroupSize);//��ȡ�������ǣ�16��16��1��
	//std::cout << LocalGroupSize[0] << LocalGroupSize[1] << LocalGroupSize[2] << std::endl;
	m_GlobalGroupSize.push_back((ElayGraphics::WINDOW_KEYWORD::getWindowWidth() + LocalGroupSize[0] - 1) / LocalGroupSize[0]);
	m_GlobalGroupSize.push_back((ElayGraphics::WINDOW_KEYWORD::getWindowHeight() + LocalGroupSize[1] - 1) / LocalGroupSize[1]);
	m_GlobalGroupSize.push_back(1);
}

void CShadingWithRSMPass::updateV()
{
	m_pShader->activeShader();
	auto ViewMatrix = ElayGraphics::Camera::getMainCameraViewMatrix();
	// ��������ӿھ����µ�λ������ ת��Ϊ �ƹ��ӿ��µ�λ������
	glm::mat4 LightVPMatrixMulInverseCameraViewMatrix = m_LightVPMatrix * glm::inverse(ViewMatrix);
	m_pShader->setMat4UniformValue("u_LightVPMatrixMulInverseCameraViewMatrix", glm::value_ptr(LightVPMatrixMulInverseCameraViewMatrix));
	glm::vec3 LightDirInViewSpace = normalize(glm::vec3(ViewMatrix * m_LightDir));
	m_pShader->setFloatUniformValue("u_LightDirInViewSpace", LightDirInViewSpace.x, LightDirInViewSpace.y, LightDirInViewSpace.z);
	// ���ü�����ɫ��
	glDispatchCompute(m_GlobalGroupSize[0], m_GlobalGroupSize[1], m_GlobalGroupSize[2]);
	// ʹ���ڴ����ϣ���ǿ�Ƹ����ǰ���ָ������ȷ�Ĵ�����ɡ�
	// glMemoryBarrier()�ܹ���֤���õ�ǰ����ָ����Դ�ķ��ʣ��Ե��õ��Ĵ���ɼ���
	//glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
}

void CShadingWithRSMPass::__initVPLsSampleCoordsAndWeights()
{
	std::default_random_engine e;
	std::uniform_real_distribution<float> u(0, 1);
	for (int i = 0; i < m_VPLNum; ++i)
	{
		float xi1 = u(e);
		float xi2 = u(e);
		m_VPLsSampleCoordsAndWeights.push_back({ xi1 * sin(2 * ElayGraphics::PI * xi2), xi1 * cos(2 * ElayGraphics::PI * xi2), xi1 * xi1, 0 });
	}

	genBuffer(GL_UNIFORM_BUFFER, m_VPLsSampleCoordsAndWeights.size() * 4 * sizeof(GL_FLOAT), m_VPLsSampleCoordsAndWeights.data(), GL_STATIC_DRAW, 1);
}
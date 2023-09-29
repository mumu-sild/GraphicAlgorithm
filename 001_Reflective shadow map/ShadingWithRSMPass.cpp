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
	// 创建我们需要的采样纹理，设置为该Shader的输出
	auto TextureConfig = std::make_shared<ElayGraphics::STexture>(); ;
	TextureConfig->InternalFormat = GL_RGBA32F;
	TextureConfig->ExternalFormat = GL_RGBA;
	TextureConfig->DataType = GL_FLOAT;
	TextureConfig->Type4MinFilter = GL_LINEAR;
	TextureConfig->Type4MagFilter = GL_LINEAR;
	TextureConfig->ImageBindUnit = 0;//将该图片绑定到0号位
	genTexture(TextureConfig);
	ElayGraphics::ResourceManager::registerSharedData("ShadingTexture", TextureConfig);
	ElayGraphics::Camera::setMainCameraMoveSpeed(0.5);
	// 创建圆面采样纹理，并保存在GL_UNIFORM_BUFFER1（binding = 1）中
	__initVPLsSampleCoordsAndWeights();

	// 
	m_LightVPMatrix = ElayGraphics::ResourceManager::getSharedDataByName<glm::mat4>("LightVPMatrix");
	// 这里使用了ComputeShader，独立于渲染管线，也用于渲染图形，可以直接将渲染结果作为渲染流水线输入
	m_pShader = std::make_shared<CShader>("ShadingWithRSM_CS.glsl");
	m_pShader->activeShader();
	// 添加Pass1中渲染的三张图
	m_pShader->setTextureUniformValue("u_AlbedoTexture", ElayGraphics::ResourceManager::getSharedDataByName<std::shared_ptr<ElayGraphics::STexture>>("AlbedoTexture"));
	m_pShader->setTextureUniformValue("u_NormalTexture", ElayGraphics::ResourceManager::getSharedDataByName<std::shared_ptr<ElayGraphics::STexture>>("NormalTexture"));
	m_pShader->setTextureUniformValue("u_PositionTexture", ElayGraphics::ResourceManager::getSharedDataByName<std::shared_ptr<ElayGraphics::STexture>>("PositionTexture"));
	// 添加Pass2中渲染的三张图
	m_pShader->setTextureUniformValue("u_RSMFluxTexture", ElayGraphics::ResourceManager::getSharedDataByName<std::shared_ptr<ElayGraphics::STexture>>("RSMFluxTexture"));
	m_pShader->setTextureUniformValue("u_RSMNormalTexture", ElayGraphics::ResourceManager::getSharedDataByName<std::shared_ptr<ElayGraphics::STexture>>("RSMNormalTexture"));
	m_pShader->setTextureUniformValue("u_RSMPositionTexture", ElayGraphics::ResourceManager::getSharedDataByName<std::shared_ptr<ElayGraphics::STexture>>("RSMPositionTexture"));
	m_pShader->setTextureUniformValue("u_RSMLightPositionTexture", ElayGraphics::ResourceManager::getSharedDataByName<std::shared_ptr<ElayGraphics::STexture>>("RSMLightPositionTexture"));
	// 设置采样半径，采样范围，采样次数
	m_pShader->setFloatUniformValue("u_MaxSampleRadius", m_MaxSampleRadius);
	m_pShader->setIntUniformValue("u_RSMSize", ElayGraphics::ResourceManager::getSharedDataByName<int>("RSMResolution"));
	m_pShader->setIntUniformValue("u_VPLNum", m_VPLNum);
	m_LightDir = glm::vec4(ElayGraphics::ResourceManager::getSharedDataByName<glm::vec3>("LightDir"), 0.0f);	//这是个方向，第四维需要是0，不能为1，否则会出问题

	std::vector<int> LocalGroupSize;
	// 用 GetProgramiv 接口传入 COMPUTE_WORK_GROUP_SIZE 参数所获得的结果是
	// 在Compute Shader中通过 
	//		layout (local_size_x = 32, local_size_y = 16, local_size_z = 1)
	// 所指定的当前计算程序的工作组大小。
	m_pShader->InquireLocalGroupSize(LocalGroupSize);//获取到的数是（16，16，1）
	//std::cout << LocalGroupSize[0] << LocalGroupSize[1] << LocalGroupSize[2] << std::endl;
	m_GlobalGroupSize.push_back((ElayGraphics::WINDOW_KEYWORD::getWindowWidth() + LocalGroupSize[0] - 1) / LocalGroupSize[0]);
	m_GlobalGroupSize.push_back((ElayGraphics::WINDOW_KEYWORD::getWindowHeight() + LocalGroupSize[1] - 1) / LocalGroupSize[1]);
	m_GlobalGroupSize.push_back(1);
}

void CShadingWithRSMPass::updateV()
{
	m_pShader->activeShader();
	auto ViewMatrix = ElayGraphics::Camera::getMainCameraViewMatrix();
	// 将摄像机视口矩阵下的位置坐标 转化为 灯光视口下的位置坐标
	glm::mat4 LightVPMatrixMulInverseCameraViewMatrix = m_LightVPMatrix * glm::inverse(ViewMatrix);
	m_pShader->setMat4UniformValue("u_LightVPMatrixMulInverseCameraViewMatrix", glm::value_ptr(LightVPMatrixMulInverseCameraViewMatrix));
	glm::vec3 LightDirInViewSpace = normalize(glm::vec3(ViewMatrix * m_LightDir));
	m_pShader->setFloatUniformValue("u_LightDirInViewSpace", LightDirInViewSpace.x, LightDirInViewSpace.y, LightDirInViewSpace.z);
	// 调用计算着色器
	glDispatchCompute(m_GlobalGroupSize[0], m_GlobalGroupSize[1], m_GlobalGroupSize[2]);
	// 使用内存屏障，以强制该语句前后的指令以正确的次序完成。
	// glMemoryBarrier()能够保证调用点前，对指定资源的访问，对调用点后的代码可见。
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
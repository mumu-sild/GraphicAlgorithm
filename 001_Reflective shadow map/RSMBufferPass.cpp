#include "RSMBufferPass.h"
#include "Shader.h"
#include "Interface.h"
#include "Common.h"
#include "Utils.h"
#include "HalfCornelBox.h"
#include <GLM/gtc/matrix_transform.hpp>
#include <GLM/gtc/type_ptr.hpp>

CRSMBufferPass::CRSMBufferPass(const std::string& vPassName, int vExcutionOrder) : IRenderPass(vPassName, vExcutionOrder)
{
}

CRSMBufferPass::~CRSMBufferPass()
{
}

void CRSMBufferPass::initV()
{
	m_pShader = std::make_shared<CShader>("RSMBuffer_VS.glsl", "RSMBuffer_FS.glsl");
	m_pHalfCornellBox = std::dynamic_pointer_cast<CHalfCornellBox>(ElayGraphics::ResourceManager::getGameObjectByName("HalfCornellBox"));
	// 三个纹理，保存RSM的位置信息、法线信息、光照信息
	auto TextureConfig4Position = std::make_shared<ElayGraphics::STexture>();
	auto TextureConfig4Normal = std::make_shared<ElayGraphics::STexture>();
	auto TextureConfig4Flux = std::make_shared<ElayGraphics::STexture>();
	auto TextureConfig4LightSpacePos = std::make_shared<ElayGraphics::STexture>();
	TextureConfig4Flux->InternalFormat = TextureConfig4Normal->InternalFormat = TextureConfig4Position->InternalFormat= TextureConfig4LightSpacePos->InternalFormat = GL_RGBA32F;
	TextureConfig4Flux->ExternalFormat = TextureConfig4Normal->ExternalFormat = TextureConfig4Position->ExternalFormat= TextureConfig4LightSpacePos->ExternalFormat = GL_RGBA;
	TextureConfig4Flux->DataType = TextureConfig4Normal->DataType = TextureConfig4Position->DataType = TextureConfig4LightSpacePos ->DataType= GL_FLOAT;
	TextureConfig4Flux->Width = TextureConfig4Normal->Width = TextureConfig4Position->Width = TextureConfig4LightSpacePos->Width = m_RSMResolution;
	TextureConfig4Flux->Height = TextureConfig4Normal->Height = TextureConfig4Position->Height = TextureConfig4LightSpacePos->Height = m_RSMResolution;
	TextureConfig4Flux->Type4WrapS = TextureConfig4Normal->Type4WrapS = TextureConfig4Position->Type4WrapS= TextureConfig4LightSpacePos->Type4WrapS = GL_CLAMP_TO_BORDER;
	TextureConfig4Flux->Type4WrapT = TextureConfig4Normal->Type4WrapT = TextureConfig4Position->Type4WrapT = TextureConfig4LightSpacePos->Type4WrapT = GL_CLAMP_TO_BORDER;
	TextureConfig4Flux->BorderColor = TextureConfig4Normal->BorderColor = TextureConfig4Position->BorderColor = TextureConfig4LightSpacePos->BorderColor = { 0,0,0,0 };

	genTexture(TextureConfig4Flux);
	genTexture(TextureConfig4Normal);
	genTexture(TextureConfig4Position);
	genTexture(TextureConfig4LightSpacePos);

	// 设置帧缓存，将该三个纹理设为渲染纹理
	m_FBO = genFBO({ TextureConfig4Flux,TextureConfig4Normal,TextureConfig4Position,TextureConfig4LightSpacePos});

	ElayGraphics::ResourceManager::registerSharedData("RSMFluxTexture", TextureConfig4Flux);
	ElayGraphics::ResourceManager::registerSharedData("RSMNormalTexture", TextureConfig4Normal);
	ElayGraphics::ResourceManager::registerSharedData("RSMPositionTexture", TextureConfig4Position);
	ElayGraphics::ResourceManager::registerSharedData("RSMLightPositionTexture", TextureConfig4LightSpacePos);

	m_pShader->activeShader();
	m_pShader->setMat4UniformValue("u_ModelMatrix", glm::value_ptr(m_pHalfCornellBox->getModelMatrix()));
	//设置光源（位置，方向）
	glm::vec3 LightPos = glm::vec3(1.5, 0.5, 0.0);	//y是高度
	glm::vec3 LightDir = glm::normalize(glm::vec3(0.03, -0.1, 0.001));	
	glm::mat4 LightViewMatrix = glm::lookAt(LightPos, LightPos + LightDir, glm::vec3(0, 1, 0));
	glm::mat4 LightProjectionMatrix = glm::ortho(-2.0f, 2.0f, -2.0f, 2.0f, 0.1f, 10.0f);
	glm::mat4 LightVPMatrix = LightProjectionMatrix * LightViewMatrix;
	m_pShader->setMat4UniformValue("u_LightVPMatrix", glm::value_ptr(LightVPMatrix));
	m_pHalfCornellBox->initModel(*m_pShader);

	ElayGraphics::ResourceManager::registerSharedData("LightVPMatrix", LightVPMatrix);
	ElayGraphics::ResourceManager::registerSharedData("RSMResolution", m_RSMResolution);
	ElayGraphics::ResourceManager::registerSharedData("LightDir", LightDir);
}

void CRSMBufferPass::updateV()
{
	glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);
	glViewport(0, 0, m_RSMResolution, m_RSMResolution);

	m_pShader->activeShader();

	m_pHalfCornellBox->updateModel(*m_pShader);

	glViewport(0, 0, ElayGraphics::WINDOW_KEYWORD::getWindowWidth(), ElayGraphics::WINDOW_KEYWORD::getWindowHeight());
	glDisable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
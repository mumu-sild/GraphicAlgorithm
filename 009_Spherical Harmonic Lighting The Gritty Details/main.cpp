#include "Interface.h"
#include "Sponza.h"
#include "ShadingPass.h"
#include "SkyBoxPass.h"
int main()
{
	ElayGraphics::WINDOW_KEYWORD::setWindowSize(1800, 900);
	ElayGraphics::WINDOW_KEYWORD::setIsCursorDisable(true);
	ElayGraphics::COMPONENT_CONFIG::setIsEnableGUI(false);
	//导入模型
	ElayGraphics::ResourceManager::registerGameObject(std::make_shared<CSponza>("Sponza", 1));
	//添加天空盒
	ElayGraphics::ResourceManager::registerRenderPass(std::make_shared<CSkyboxPass>("SkyboxPass", 1));
	//添加
	ElayGraphics::ResourceManager::registerRenderPass(std::make_shared<CShadingPass>("ShadingPass", 2));
	ElayGraphics::App::initApp();

	ElayGraphics::App::updateApp();

	return 0;
}
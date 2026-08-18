#include "SceneManager.h"
#include "utility/Common.h"
#include "ISceneBase.h"

#include "network/Network.h"

extern network::Network g_Network;

namespace scene
{
	SceneManager::SceneManager()
		:mCurrentScene(nullptr)
	{
	}
	SceneManager* const SceneManager::GetInstance()
	{
		static SceneManager instance;
		return &instance;
	}

	void SceneManager::InitScene(ISceneBase* const scene)
	{
		RT_ASSERT(mCurrentScene == nullptr);
		mCurrentScene = scene;
	}

	void SceneManager::Update()
	{
		while (utility::Message* msg = g_Network.PopPacket())
		{
			bool bSuccess = mCurrentScene->PacketProc(*msg);
			delete msg;
			if (!bSuccess)
			{
				break;
			}
		}

		RT_ASSERT(mCurrentScene != nullptr);
		mCurrentScene->update();
		ISceneBase* nextScene = mCurrentScene->getNextSceneOrNull();
		if (nextScene)
		{
			delete mCurrentScene;
			mCurrentScene = nextScene;
		}
	}

	void SceneManager::Render()
	{
		RT_ASSERT(mCurrentScene != nullptr);
		mCurrentScene->render();
	}
};
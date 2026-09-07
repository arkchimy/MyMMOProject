#pragma once
namespace scene
{
	class ISceneBase;
}
namespace scene
{
	class SceneManager final
	{
	private:
		SceneManager();
	public:
		SceneManager(const SceneManager&) = delete;
		SceneManager& operator=(const SceneManager&) = delete;
		SceneManager(SceneManager&&) = delete;
		SceneManager& operator=(SceneManager&&) = delete;

		static SceneManager* const GetInstance();
		void InitScene(ISceneBase* const scene);

		void Update();
		void Render();

	private:
		ISceneBase* mCurrentScene;
	};

}
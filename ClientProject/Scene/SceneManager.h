#pragma once
namespace scene
{
	class ISceneBase;
}
namespace scene
{
	class SceneManager
	{
	private:
		SceneManager();
	public:
		static SceneManager* const GetInstance();
		void InitScene(ISceneBase* const scene);

		void Update();
		void Render();

	private:
		ISceneBase* mCurrentScene;
	};

}
#pragma once

namespace utility
{
	class Message;
}

namespace scene
{
	class ISceneBase
	{
		friend class SceneManager;
	public:
		virtual ~ISceneBase() = default;
	private:
		virtual void update() = 0;
		virtual void render() = 0;
		virtual ISceneBase* getNextSceneOrNull() = 0;
		virtual bool PacketProc(utility::Message& msg) = 0;
	};

}

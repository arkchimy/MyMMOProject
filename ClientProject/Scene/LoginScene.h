#pragma once
#include "ISceneBase.h"

namespace ui
{
	class UIBase;
}
namespace utility
{
	class Message;
}
namespace scene
{
	enum login_config
	{
		LOGIN_CONFIG_UI_LEN = 1
	};
	class LoginScene :public ISceneBase
	{
	public:
		LoginScene();
		~LoginScene();
	private:
		void startInit();
		virtual void update();
		virtual void render();
		virtual ISceneBase* getNextSceneOrNull();
		bool PacketProc(utility::Message& msg);

		void sendFieldEnterReq(const char* id, const char* pw);
		void onAuthFailRes(utility::Message& msg);
		void onAuthRes(utility::Message& msg);
	private:
		ISceneBase* mNextScene;
		ui::UIBase* mUIArr[LOGIN_CONFIG_UI_LEN];
		bool mSendFlag;

		char mIPAddress[16]{};
		short mPort;
		char mID[20]{}; // 널문자 포함 20바이트 — Player::mNickname(char[20])과 크기 일치, id를 그대로 닉네임으로 재사용
		char mPW[64]{};
	};
}


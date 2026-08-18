#include "Network/Network.h"
#include "LoginScene.h"
#include "FieldScene.h"
#include "Actors/Player.h"
#include "UI/UIBase.h"
#include "UI/HPBar.h"

#include "utility/Message.h"

#include "../../_Shared/Protocol.h"
#include <iostream>
#include <iomanip>

namespace scene
{
	LoginScene::LoginScene()
		:mNextScene(nullptr)
		, mSendFlag(false)
		, mPort(0)
	{
		mUIArr[0] = new ui::HPBar();
		startInit();
	}
	LoginScene::~LoginScene()
	{
		for (int i = 0; i < LOGIN_CONFIG_UI_LEN; ++i)
		{
			if (mUIArr != nullptr)
			{
				delete mUIArr[i];
			}
		}
	}
	void LoginScene::startInit()
	{
		IP_AND_PORT:
			std::cout << "IP Address :";
			std::cin >> std::setw(16) >> mIPAddress;

			std::cout << "Port : ";
			std::cin >> mPort;

			std::cout << "ID : ";
			std::cin >> std::setw(20) >> mID;

			std::cout << "PW : ";
			std::cin >> std::setw(64) >> mPW;

			if (std::cin.fail())
			{
				if (std::cin.eof())
				{
					std::cin.clear();
					goto IP_AND_PORT;
				}
				std::cin.clear();
				std::string temp;
				std::cin >> temp;

				goto IP_AND_PORT;
			}

		
	}
	void LoginScene::update()
	{
		Sleep(1000);
	/*	if (GetAsyncKeyState(VK_SPACE) & 0x8000)
		{*/
			//성공 시
			if (g_Network.Connect(mIPAddress, mPort))
			{
				if (!mSendFlag)
				{
					sendFieldEnterReq(mID, mPW);
				}
			}
		//}
		for (int i = 0; i < LOGIN_CONFIG_UI_LEN; ++i)
		{
			mUIArr[i]->Update();
		}
	}
	void LoginScene::render()
	{
		for (int i = 0; i < LOGIN_CONFIG_UI_LEN; ++i)
		{
			mUIArr[i]->Render();
		}
	}
	ISceneBase* LoginScene::getNextSceneOrNull()
	{
		return mNextScene;
	}
	bool LoginScene::PacketProc(utility::Message& msg)
	{
		__int16 type;
		msg >> type;

		switch ((PacketType)type)
		{
		case PacketType::FIELD_AUTH_FAIL:
			onAuthFailRes(msg);
			return false;
			break;
		case PacketType::FIELD_AUTH_RES:
			onAuthRes(msg);
			return false;
			break;
		default:
			RT_ASSERT(false);
		}
		return true;
	}
	void LoginScene::sendFieldEnterReq(const char* id, const char* pw)
	{
		PacketType::FIELD_AUTH_REQ;
		Header header;
		header.Len = sizeof(__int16) + sizeof(mID) + sizeof(mPW);
		header.RandKey = 0;

		utility::Message msg;
		msg.PutData(&header, sizeof(header));
		msg << static_cast<__int16>(PacketType::FIELD_AUTH_REQ);
		msg.PutData(const_cast<char*>(id), sizeof(mID));
		msg.PutData(const_cast<char*>(pw), sizeof(mPW));
		if (g_Network.Send(msg))
		{
			mSendFlag = true;
		}
	}
	void LoginScene::onAuthFailRes(utility::Message& msg)
	{
		PacketType::FIELD_AUTH_FAIL;
		__int8 result;
		msg >> result;
		mSendFlag = false;
		g_Network.Disconnect();
	}
	void LoginScene::onAuthRes(utility::Message& msg)
	{
		PacketType::FIELD_AUTH_RES;
		__int64 characterID;
		float posX;
		float posY;

		msg >> characterID;
		msg >> posX;
		msg >> posY;

		actors::Player* player = new actors::Player(characterID,posX,posY);
		mNextScene = new FieldScene(player);
	}
};
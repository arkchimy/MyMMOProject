#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <cstdlib>
#ifdef WIN32_LEAN_AND_MEAN
#else
#define WIN32_LEAN_AND_MEAN

#endif
#include <WS2tcpip.h>
#include <Windows.h>
#include <mmsystem.h>
#include <thread>

#include "ClientBot.h"
#include "Common.h"
#include "Session.h"
#include "Packet.h"
#include "PacketStats.h"
#include "../_lib/MTProfiler_Lib/MTProfiler_Lib.h"
#include "../_Shared/AttackConfig.h"

#pragma comment(lib,"ws2_32")
#pragma comment(lib,"winmm.lib")
//송신버퍼 문제같다.
extern thread_local CProfileManager manager;

struct MonitorPacketTrack
{
	PacketType type;
	const char* name;
	int group;
};

//UTF-8 한글(3바이트, 화면 2칸)과 setw()의 바이트 기준 패딩이 어긋나는 걸 보정.
//한글 글자 수만큼 setw에 더해줘야 화면상 실제 폭이 width에 맞춰짐.
static int KoreanPad(const char* str, int width)
{
	int extra = 0;
	for (const unsigned char* p = reinterpret_cast<const unsigned char*>(str); *p; ++p)
	{
		if ((*p & 0xF0) == 0xE0)	//UTF-8 3바이트 시퀀스 시작 바이트 (한글 음절 범위)
		{
			++extra;
		}
	}
	return width + extra;
}

//box-drawing 문자(─│┌┬┐├┼┤└┴┘)는 UTF-8로 3바이트지만 화면 폭은 1칸이라 KoreanPad 보정이 필요 없음.
static std::string HLine(int width, const char* seg)
{
	std::string line;
	for (int i = 0; i < width; ++i)
	{
		line += seg;
	}
	return line;
}

static std::string BuildBorder(const int* widths, int colCount, const char* left, const char* mid, const char* right)
{
	std::string line = left;
	for (int i = 0; i < colCount; ++i)
	{
		line += HLine(widths[i], "─");
		line += (i + 1 < colCount ? mid : right);
	}
	return line;
}

struct KVRow
{
	std::string label;
	std::string value;
};

//4개 박스를 한 줄에 나란히 배치하기 위해 폭을 고정값으로 공유 (박스마다 폭이 다르면 옆으로 못 붙임)
constexpr int kKVLabelWidth = 16;
constexpr int kKVValueWidth = 10;
constexpr int kKVBoxWidth = kKVLabelWidth + kKVValueWidth + 3;	//│라벨│값│

//"항목/값" 2열 박스 테이블을 한 줄씩 문자열로 만들어 반환 (직접 출력하지 않음 - 옆 박스와 나란히 찍기 위해)
static std::vector<std::string> BuildKVBox(const char* title, const std::vector<KVRow>& rows)
{
	const int widths[2] = { kKVLabelWidth, kKVValueWidth };
	std::vector<std::string> lines;

	std::ostringstream titleLine;
	titleLine << std::left << std::setw(KoreanPad(title, kKVBoxWidth)) << title;
	lines.push_back(titleLine.str());

	lines.push_back(BuildBorder(widths, 2, "┌", "┬", "┐"));

	std::ostringstream header;
	header << "│" << std::left << std::setw(KoreanPad("항목", kKVLabelWidth)) << "항목"
		<< "│" << std::setw(KoreanPad("값", kKVValueWidth)) << "값" << "│";
	lines.push_back(header.str());

	lines.push_back(BuildBorder(widths, 2, "├", "┼", "┤"));

	for (const KVRow& row : rows)
	{
		std::ostringstream rowStream;
		rowStream << "│" << std::left << std::setw(KoreanPad(row.label.c_str(), kKVLabelWidth)) << row.label
			<< "│" << std::right << std::setw(kKVValueWidth) << row.value << "│";
		lines.push_back(rowStream.str());
	}

	lines.push_back(BuildBorder(widths, 2, "└", "┴", "┘"));
	return lines;
}

//여러 박스(BuildKVBox/그룹 테이블 결과)를 가로로 나란히 출력. 줄 수가 다른 박스는 빈 칸으로 높이를 맞춤.
//boxWidth: blocks 안 모든 줄의 공통 시각적 폭. 박스 스타일마다 폭이 달라서(KV박스/타입박스) 호출자가 넘겨줘야 함
//- 안 맞으면 먼저 끝난 박스의 빈 자리 패딩이 짧아져 다음 박스가 밀림.
static void PrintBoxesSideBySide(std::ostream& out, const std::vector<std::vector<std::string>>& blocks, int boxWidth)
{
	size_t maxLines = 0;
	for (const std::vector<std::string>& block : blocks)
	{
		maxLines = (block.size() > maxLines) ? block.size() : maxLines;
	}

	for (size_t r = 0; r < maxLines; ++r)
	{
		for (const std::vector<std::string>& block : blocks)
		{
			if (r < block.size())
			{
				out << block[r];
			}
			else
			{
				out << std::string(boxWidth, ' ');	//이미 끝난 박스 자리는 공백으로 채워 다음 박스 위치를 맞춤
			}
			out << "  ";	//박스 사이 여백
		}
		out << "\n";
	}
	out << "\n";
}

std::ostream& operator>>(std::ostream& out, const ClientBot& bot)
{
	static MonitorPacketTrack sPacketTrackTypes[] =
	{
		{ PacketType::FIELD_AUTH_REQ, "인증 요청", 0 },
		{ PacketType::FIELD_AUTH_RES, "인증 성공", 0 },
		{ PacketType::FIELD_AUTH_FAIL, "인증 실패", 0 },

		{ PacketType::OTHER_CHARACTER_SPAWN, "캐릭터 스폰", 1 },
		{ PacketType::OTHER_CHARACTER_SPAWN_BATCH, "캐릭터 스폰(배치)", 1 },
		{ PacketType::CHARACTER_DESPAWN, "캐릭터 디스폰", 1 },
		{ PacketType::MOVE_START, "이동 시작", 1 },
		{ PacketType::MOVE_STOP, "이동 정지", 1 },
		{ PacketType::PLAYER_ATTACK_REQ, "공격 요청", 1 },
		{ PacketType::OTHER_CHARACTER_ATTACK, "공격 브로드캐스트", 1 },
		{ PacketType::CHARACTER_DAMAGED, "플레이어 피격", 1 },

		{ PacketType::MONSTER_SPAWN, "몬스터 스폰", 2 },
		{ PacketType::MONSTER_SPAWN_BATCH, "몬스터 스폰(배치)", 2 },
		{ PacketType::MONSTER_DESPAWN, "몬스터 디스폰", 2 },
		{ PacketType::MONSTER_MOVE_START, "몬스터 이동 시작", 2 },
		{ PacketType::MONSTER_MOVE_STOP, "몬스터 이동 정지", 2 },
		{ PacketType::MONSTER_DAMAGED, "몬스터 피격", 2 },
		{ PacketType::MONSTER_ATTACK, "몬스터 공격", 2 },

		{ PacketType::ITEM_SPAWN, "아이템 스폰", 3 },
		{ PacketType::ITEM_SPAWN_BATCH, "아이템 스폰(배치)", 3 },
		{ PacketType::ITEM_DESPAWN, "아이템 디스폰", 3 },
		{ PacketType::LOOT_REQ, "루팅 요청", 3 },
		{ PacketType::LOOT_RES, "루팅 응답", 3 },
	};
	constexpr int packetTrackCnt = sizeof(sPacketTrackTypes) / sizeof(sPacketTrackTypes[0]);
	static const char* kGroupNames[] = { "인증", "캐릭터", "몬스터", "아이템" };

	//초당값 계산용 이전 누적치 (호출자가 monitorThread 하나뿐이므로 static으로 충분)
	static int64_t prevSend[packetTrackCnt] = {};
	static int64_t prevRecv[packetTrackCnt] = {};
	static int64_t prevTotalSend = 0;
	static int64_t prevTotalRecv = 0;

	int64_t totalSend = stats::GetTotalSendCount();
	int64_t totalRecv = stats::GetTotalRecvCount();
	int64_t sendPerSec = totalSend - prevTotalSend;
	int64_t recvPerSec = totalRecv - prevTotalRecv;
	prevTotalSend = totalSend;
	prevTotalRecv = totalRecv;

	SYSTEMTIME st;
	GetLocalTime(&st);

	out << "==== ClientBot [" << std::setfill('0')
		<< std::setw(2) << st.wHour << ":" << std::setw(2) << st.wMinute << ":" << std::setw(2) << st.wSecond
		<< "] ====\n" << std::setfill(' ');

	PrintBoxesSideBySide(out, {
		BuildKVBox("자원", {
			{ "접속중", std::to_string(bot.mConnectedCount) },
			{ "인증대기", std::to_string(bot.mAuthWaitCount) },
		}),
		BuildKVBox("Disconnect", {
			{ "접속종료", std::to_string(bot.mDisconnectCount) },
			{ "사망재접속", std::to_string(bot.mReconnectDeadCount) },
			{ "누적접속시도", std::to_string(bot.mTotalConnectCount) },
		}),
		BuildKVBox("TPS", {
			{ "초당송신", std::to_string(sendPerSec) },
			{ "초당수신", std::to_string(recvPerSec) },
			{ "송신누적", std::to_string(totalSend) },
			{ "수신누적", std::to_string(totalRecv) },
		}),
	}, kKVBoxWidth);

	constexpr int kItemGroup = 3;	//kGroupNames[3] == "아이템"
	constexpr int kTypeColWidth = 20;
	constexpr int kNumColWidth = 10;
	const int kColWidths[5] = { kTypeColWidth, kNumColWidth, kNumColWidth, kNumColWidth, kNumColWidth };
	constexpr int kTypeBoxWidth = kTypeColWidth + kNumColWidth * 4 + 6;	//│종류│송신│초당송신│수신│초당수신│ (파이프 6개)

	//그룹 하나(인증/캐릭터/몬스터/아이템)를 박스 테이블 문자열 줄 배열로 생성 (sPacketTrackTypes를 그룹별로 필터링)
	auto buildGroupLines = [&](int groupId, const char* title) -> std::vector<std::string>
	{
		std::vector<std::string> lines;

		std::ostringstream titleLine;
		titleLine << std::left << std::setw(KoreanPad(title, kTypeBoxWidth)) << title;
		lines.push_back(titleLine.str());

		lines.push_back(BuildBorder(kColWidths, 5, "┌", "┬", "┐"));

		std::ostringstream header;
		header << "│" << std::left << std::setw(KoreanPad("종류", kTypeColWidth)) << "종류"
			<< "│" << std::setw(KoreanPad("송신", kNumColWidth)) << "송신"
			<< "│" << std::setw(KoreanPad("초당송신", kNumColWidth)) << "초당송신"
			<< "│" << std::setw(KoreanPad("수신", kNumColWidth)) << "수신"
			<< "│" << std::setw(KoreanPad("초당수신", kNumColWidth)) << "초당수신" << "│";
		lines.push_back(header.str());

		lines.push_back(BuildBorder(kColWidths, 5, "├", "┼", "┤"));

		for (int i = 0; i < packetTrackCnt; ++i)
		{
			if (sPacketTrackTypes[i].group != groupId)
			{
				continue;
			}
			int64_t sendCnt = stats::GetSendCount(sPacketTrackTypes[i].type);
			int64_t recvCnt = stats::GetRecvCount(sPacketTrackTypes[i].type);
			std::ostringstream row;
			row << "│" << std::left << std::setw(KoreanPad(sPacketTrackTypes[i].name, kTypeColWidth)) << sPacketTrackTypes[i].name
				<< "│" << std::right << std::setw(kNumColWidth) << sendCnt
				<< "│" << std::setw(kNumColWidth) << (sendCnt - prevSend[i])
				<< "│" << std::setw(kNumColWidth) << recvCnt
				<< "│" << std::setw(kNumColWidth) << (recvCnt - prevRecv[i]) << "│";
			lines.push_back(row.str());
			prevSend[i] = sendCnt;
			prevRecv[i] = recvCnt;
		}

		lines.push_back(BuildBorder(kColWidths, 5, "└", "┴", "┘"));
		if (groupId == kItemGroup)
		{
			lines.push_back("루팅 성공 누적 : " + std::to_string(bot.mTotalRootItemCount));
		}
		return lines;
	};

	//아이템을 인증 옆으로, 몬스터를 캐릭터 옆으로 붙여서 세로 길이를 줄임
	PrintBoxesSideBySide(out, { buildGroupLines(0, kGroupNames[0]), buildGroupLines(3, kGroupNames[3]) }, kTypeBoxWidth);
	PrintBoxesSideBySide(out, { buildGroupLines(1, kGroupNames[1]), buildGroupLines(2, kGroupNames[2]) }, kTypeBoxWidth);

	return out;
}

ClientBot::ClientBot()
	:mPort(0)
	, wsa(nullptr)
	, mMonitorOn(true)
	, mAuthWaitCount(0)
	, mDisconnectCount(0)
	, mReconnectDeadCount(0)
	, mConnectedCount(0)
	, mTotalConnectCount(0)
	, mTotalRootItemCount(0)
{
	SetConsoleOutputCP(CP_UTF8);
IP_AND_PORT:
	std::cout << "IP Address :";
	std::cin >> std::setw(16) >> mIPAddress;

	std::cout << "Port : ";
	std::cin >> mPort;

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
	wsa = new RAIIwsadata();

	int userCnt;
	startInput(userCnt);

	mThreadCnt = (userCnt - 1) / 64 + 1;

	for (int i = 0; i < mThreadCnt; ++i)
	{
		//마지막 스레드만 나머지를 담당 (예: userCnt=100 → 스레드0: 64, 스레드1: 36)
		int sessionCnt = (i == mThreadCnt - 1) ? (userCnt - 64 * i) : 64;

		std::wstring threadName = L"SelectThread";
		threads[i] = std::thread(&ClientBot::selectThread, this, sessionCnt);
		std::wstring str = L"1";
		threadName += str;
		SetThreadDescription(threads[i].native_handle(), threadName.c_str());
	}

	mMonitorThread = std::thread(&ClientBot::monitorThread, this);
}

ClientBot::~ClientBot()
{
	HANDLE Handles[20]{};
	for (int32_t idx = 0; idx < mThreadCnt; ++idx)
	{
		Handles[idx] = threads[idx].native_handle();
	}

	WaitForMultipleObjects(mThreadCnt, Handles, true, INFINITE);
	for (int32_t idx = 0; idx < mThreadCnt; ++idx)
	{
		threads[idx].join();
	}
	mMonitorOn = false;
	mMonitorThread.join();

}

void ClientBot::connectSessions(std::shared_ptr<Session[]>& sessions, int32_t sessionCnt)
{
	SOCKADDR_IN serverAddr;
	ZeroMemory(&serverAddr, sizeof(serverAddr));

	serverAddr.sin_family = AF_INET;
	//오류가 발생하지 않으면 InetPton 함수는 값 1을 반환
	RT_ASSERT(inet_pton(AF_INET, mIPAddress, &serverAddr.sin_addr) == 1, "inet_pton 실패 ");
	serverAddr.sin_port = htons(mPort);

	u_long iMode = 1;

	for (int i = 0; i < sessionCnt; ++i)
	{
		sessions[i].Init();	//소켓 생성 + 송수신 링버퍼 할당
		RT_ASSERT(sessions[i].mSock != INVALID_SOCKET, "INVALID_SOCKET");

		//성공적으로 완료되면 ioctlsocket 은 0을 반환합니다.
		RT_ASSERT(ioctlsocket(sessions[i].mSock, FIONBIO, &iMode) == 0, "ioctlsocket 실패");

		//오류가 발생하지 않으면 connect 는 0을 반환합니다.
		int retval = connect(sessions[i].mSock, (const sockaddr*)&serverAddr, sizeof(serverAddr));
		InterlockedIncrement64(&mTotalConnectCount);
		if (retval != 0)
		{
			RT_ASSERT(WSAGetLastError() == WSAEWOULDBLOCK, "Connect 우드블락이 아닌 에러 발생");
		}
		//성공/실패 판정은 select 루프에 일임하고, 여기서는 상태만 기록
		sessions[i].mSessionState = eSessionState::REQ_CONNECT;
	}
}

void ClientBot::connectSession(std::shared_ptr<Session[]>& sessions, int32_t i)
{
	SOCKADDR_IN serverAddr;
	ZeroMemory(&serverAddr, sizeof(serverAddr));

	serverAddr.sin_family = AF_INET;
	//오류가 발생하지 않으면 InetPton 함수는 값 1을 반환
	RT_ASSERT(inet_pton(AF_INET, mIPAddress, &serverAddr.sin_addr) == 1, "inet_pton 실패 ");
	serverAddr.sin_port = htons(mPort);

	u_long iMode = 1;

	sessions[i].Init();	//소켓 생성 + 송수신 링버퍼 할당
	RT_ASSERT(sessions[i].mSock != INVALID_SOCKET, "INVALID_SOCKET");

	//성공적으로 완료되면 ioctlsocket 은 0을 반환합니다.
	RT_ASSERT(ioctlsocket(sessions[i].mSock, FIONBIO, &iMode) == 0, "ioctlsocket 실패");

	//오류가 발생하지 않으면 connect 는 0을 반환합니다.
	int retval = connect(sessions[i].mSock, (const sockaddr*)&serverAddr, sizeof(serverAddr));
	InterlockedIncrement64(&mTotalConnectCount);
	if (retval != 0)
	{
		RT_ASSERT(WSAGetLastError() == WSAEWOULDBLOCK, "Connect 우드블락이 아닌 에러 발생");
	}
	//성공/실패 판정은 select 루프에 일임하고, 여기서는 상태만 기록
	sessions[i].mSessionState = eSessionState::REQ_CONNECT;
	
}

bool ClientBot::selectLogic(std::shared_ptr<Session[]>& sessions, int32_t sessionCnt)
{
	FD_SET readSet{};
	FD_SET writeSet{};
	FD_SET exceptSet{};

	FD_ZERO(&readSet);
	FD_ZERO(&writeSet);
	FD_ZERO(&exceptSet);
	int32_t currentTime = timeGetTime();

	for (int i = 0; i < sessionCnt; ++i)
	{
		switch (sessions[i].mSessionState)
		{
		case eSessionState::REQ_CONNECT:
			FD_SET(sessions[i].mSock, &writeSet);
			FD_SET(sessions[i].mSock, &exceptSet);
			break;
		case eSessionState::CONNECTED:
		case eSessionState::AUTHED:
			if (sessions[i].mState == ePlayerState::Dead)
			{
				if (500  <= sessions[i].GetDeadFrame()) // 10초
				{
					sessions[i].Disconnect(currentTime);
					InterlockedIncrement64(&mReconnectDeadCount);
					InterlockedDecrement64(&mConnectedCount);
					continue;
				}
			}
			FD_SET(sessions[i].mSock, &readSet);
			if (0 < sessions[i].mSendSize)
			{
				FD_SET(sessions[i].mSock, &writeSet);	//보낼 게 있을 때만 (busy loop 방지)
			}
		
			break;
		case eSessionState::DISCONNECTED:
			// 연결이 끊긴지 1초가 넘었다면
			if (sessions[i].mDisconnectTime + 1000 <= currentTime)
			{
				connectSession(sessions, i);
				FD_SET(sessions[i].mSock, &writeSet);
				FD_SET(sessions[i].mSock, &exceptSet);
			}
			break;
		}
	}

	timeval time_val{ 0,0 };
	int retCnt = select(0, &readSet, &writeSet, &exceptSet, &time_val);
	if (retCnt < 0)
	{
		return false;
	}
	RT_ASSERT(retCnt != SOCKET_ERROR, "select 실패");

	for (int i = 0; i < sessionCnt; ++i)
	{
		if (retCnt == 0)
		{
			break;
		}
		switch (sessions[i].mSessionState)
		{
		case eSessionState::REQ_CONNECT:
			if (FD_ISSET(sessions[i].mSock, &exceptSet))
			{
				--retCnt;
				//connect 실패
				closesocket(sessions[i].mSock);
				sessions[i].mSock = INVALID_SOCKET;
				sessions[i].mSessionState = eSessionState::DISCONNECTED;
			}
			else if (FD_ISSET(sessions[i].mSock, &writeSet))
			{
				--retCnt;
				//connect 성공
				sessions[i].mSessionState = eSessionState::CONNECTED;
				sessions[i].mState = ePlayerState::Idle;
				sessions[i].PostFieldAuthReq();	//입장 인증 요청 (접속 직후 1회)
				InterlockedIncrement64(&mAuthWaitCount);
				InterlockedIncrement64(&mConnectedCount);
			}
			break;
		case eSessionState::CONNECTED:
		case eSessionState::AUTHED:
			if (FD_ISSET(sessions[i].mSock, &writeSet))
			{
				--retCnt;
				sessions[i].PostSend();
			}
			if (FD_ISSET(sessions[i].mSock, &readSet))
			{
				--retCnt;
				if (sessions[i].Recv() == false)
				{
					//서버가 끊음 (또는 강제 종료)
					if (sessions[i].mSessionState == eSessionState::CONNECTED)
					{
						InterlockedDecrement64(&mAuthWaitCount);	//응답 못 받고 끊긴 세션은 대기 목록에서 제거
					}
					InterlockedIncrement64(&mDisconnectCount);
					InterlockedDecrement64(&mConnectedCount);
					sessions[i].Disconnect(currentTime);
				}
				else
				{
					recvPacketProc(sessions[i]);
				}
			}
			break;
		case eSessionState::DISCONNECTED:
			break;
		}
	}
	return true;
}

void ClientBot::monitorThread() const
{

	HWND hwnd = GetConsoleWindow();
	constexpr int x = 1200;
	constexpr int y = 50;
	MoveWindow(hwnd, x, y, 1000, 800, TRUE);
	system(" mode  con lines=40   cols=150 ");
	DWORD currentTime = timeGetTime();
	DWORD nextTime = currentTime + 1000;
	while (mMonitorOn)
	{
		currentTime = timeGetTime();
		if (nextTime <= currentTime)
		{
			std::cout >> *this;
			nextTime += 1000;
		}
		else
		{
			Sleep(nextTime - currentTime);
		}
	}
}



void ClientBot::selectThread(int32_t sessionCnt)
{
	srand(GetCurrentThreadId());	//스레드별 시드 (전 스레드 동일 시퀀스 방지)
	CProfileRegistry::GetInstance().RegistProfiler(&manager);


	std::shared_ptr<Session[]> sessions = std::make_shared<Session[]>(sessionCnt);
	connectSessions(sessions, sessionCnt);

	// 프레임 변수
	DWORD startTime = timeGetTime();
	DWORD nextTime = startTime + 20;
	DWORD frameTime = startTime + 1000;

	int frameCnt = 0;

	while (1)
	{
		Profiler profile(L"Frame_Total");
		startTime = timeGetTime();
		if (nextTime <= startTime)
		{
			for (int i = 0; i < sessionCnt; ++i)
			{
				if (sessions[i].mSessionState == eSessionState::AUTHED)
				{
					sessions[i].Update();
				}
			}
			nextTime += 20;
		}
		if (selectLogic(sessions, sessionCnt) == false)
		{
			break;
		}
		++frameCnt;

		startTime = timeGetTime();
		if (frameTime <= startTime)
		{
			//std::cout << GetCurrentThreadId() << "Frame :" << frameCnt << "\n";
			frameTime += 1000;
			frameCnt = 0;
		}
		if (startTime < nextTime)
		{
			Sleep(nextTime - startTime);
		}
	}
}

void ClientBot::recvPacketProc(Session& session)
{
	while (sizeof(Header) <= session.mRecvSize)
	{
		Header header;
		memcpy(&header, session.mRecvBuffer, sizeof(header));
		//서버가 보낸 패킷이 깨졌다면 봇이 판별할 수 있는 가장 이른 지점
		RT_ASSERT(0 < header.Len, "잘못된 header.Len");

		int packetSize = sizeof(header) + header.Len;
		if (session.mRecvSize < packetSize)
		{
			break;	//미완성 패킷 : 다음 recv 후 재시도
		}

		packetProc(session, session.mRecvBuffer + sizeof(header), header.Len);

		//처리한 패킷만큼 앞으로 당기기
		memmove(session.mRecvBuffer, session.mRecvBuffer + packetSize, session.mRecvSize - packetSize);
		session.mRecvSize -= packetSize;
	}
}

void ClientBot::packetProc(Session& session, const char* payload, int len)
{
	int16_t type;
	memcpy(&type, payload, sizeof(type));
	stats::RecordRecv(static_cast<PacketType>(type));

	switch ((PacketType)type)
	{
	case PacketType::FIELD_AUTH_RES:
	{
		{
			RT_ASSERT(len == sizeof(FieldAuthResPayload), "AUTH_RES 크기 불일치");

			FieldAuthResPayload res;
			memcpy(&res, payload, sizeof(res));
			handleFieldAuth(session, res);
			break;
		}
	}
	case PacketType::FIELD_AUTH_FAIL:
		RT_ASSERT(false, "인증 실패 응답 : accountNo 중복 발급 버그?");
		break;
	case PacketType::MONSTER_MOVE_STOP:
	{
		RT_ASSERT(len == sizeof(MoveStopPayload), "MONSTER_MOVE_STOP 크기 불일치");
		MoveStopPayload res;
		memcpy(&res, payload, sizeof(res));
		handleMonsterMoveStop(session, res);
		break;
	}
	// TODO : 자신이 노리고 있는 몬스터가 맞았다면 그쪽으로 이동해 Attack하기.
	case PacketType::MONSTER_DAMAGED:
	{
		RT_ASSERT(len == sizeof(MonsterDamaged), "MonsterDamaged 크기 불일치");
		MonsterDamaged res;
		memcpy(&res, payload, sizeof(res));
		handleMonsterDamaged(session, res);

		break;
	}
	case PacketType::CHARACTER_DAMAGED:
	{
		RT_ASSERT(len == sizeof(CharacterDamaged), "CharacterDamaged 크기 불일치");
		CharacterDamaged res;
		memcpy(&res, payload, sizeof(res));
		handleCharecterDamaged(session, res);
		break;
	}
	// TODO : 전투 중이 아니라면 아이템 쪽으로 이동.
	case PacketType::ITEM_SPAWN:
	{
		RT_ASSERT(len == sizeof(ItemSpawn), "ItemSpawn 크기 불일치");
		ItemSpawn res;
		memcpy(&res, payload, sizeof(res));
		handleItemSpawn(session, res);
		break;
	}

	// TODO : 성공하였다면 아이템 카운트 증가, 실패하였다면 아무짓도 안 함.
	case PacketType::LOOT_RES:
	{
		RT_ASSERT(len == sizeof(LootRes), "LootRes 크기 불일치");
		LootRes res;
		memcpy(&res, payload, sizeof(res));
		handleLootRes(session, res);
		break;
	}
	default:
		break;	//SPAWN/MOVESTART/BATCH 계열은 무시. 
	}
}

void ClientBot::handleFieldAuth(Session& session, const FieldAuthResPayload& res)
{
	// 위치저장 검증: mPos를 덮어쓰기 전에, disconnect 시점 위치(아직 mPos에 남아있음)와
	// DB에서 로딩된 위치(res)를 비교. %f로 DB에 넣었다 atof로 다시 읽는 과정에서
	// 소수점 6자리로 잘리므로 정확히 == 비교 대신 엡실론 비교.
	if (session.mHasDisconnectPos)
	{
		map::Position loadedPos{ res.x, res.y };
		float dist = session.mPos - loadedPos;	// map::Position::operator- : 유클리드 거리
		std::string msg = "위치저장 검증 실패 AccountNo:" + std::to_string(session.mAccountNo)
			+ " disconnect(" + std::to_string(session.mPos.x) + "," + std::to_string(session.mPos.y) + ")"
			+ " != loaded(" + std::to_string(res.x) + "," + std::to_string(res.y) + ")"
			+ " dist=" + std::to_string(dist);
		RT_ASSERT(dist <= session.mSpeed * ALLOW_DELAY_FRAME, msg);
	}

	session.ConnectComplete();
	session.mCharacterId = res.characterId;
	session.mPos.x = res.x;
	session.mPos.y = res.y;
	session.mSessionState = eSessionState::AUTHED;	//필드 입장 완료
	session.mHasDisconnectPos = true;	// mPos는 그대로 둠 — 재접속 후 handleFieldAuth에서 비교용으로 씀
	InterlockedDecrement64(&mAuthWaitCount);
}


void ClientBot::handleMonsterMoveStop(Session& session, const MoveStopPayload& packet)
{
	// Player가 딱히 하는것이 없다면, 추적을 시작하자. 전투 준비
	switch (session.mState)
	{
	case ePlayerState::Idle:
	case ePlayerState::Chase:
	case ePlayerState::Attack:
		session.ChaseTarget(packet.characterId, map::Position{ packet.x, packet.y });
		break;
	default:
		break;
	}
}

void ClientBot::handleMonsterDamaged(Session& session, const MonsterDamaged& packet)
{

	if (packet.monsterId == session.mTargetID && packet.hp <= 0)
	{
		session.MonsterChaseInit();
		session.mMonsterKillTime = 0;
		return;
	}

	if (session.mState == ePlayerState::Root)
	{
		return;
	}

	if (packet.monsterId != session.mTargetID)
	{
		return;
	}



}

void ClientBot::handleCharecterDamaged(Session& session, const CharacterDamaged& packet)
{
	if (packet.characterId != session.mCharacterId)
	{
		return;
	}
	session.TakeDamage(packet.hp, packet.x, packet.y);

	if (session.mState == ePlayerState::Dead)
	{
		return;   // 이 피격으로 죽었거나 이미 죽어있었으면 반격 안 함
	}
	// 반격: 날 때린 몬스터를 추적 대상으로 (이미 다른 대상을 쫓는 중이면 ChaseTarget 내부에서 무시됨)
	session.ChaseTarget(packet.monsterId, session.mPos);
}

void ClientBot::handleItemSpawn(Session& session, const ItemSpawn& packet)
{
	if (session.mTargetItemID == -1)
	{
		session.mTargetItemID = packet.itemUniqueId;
		session.ChaseItem(packet.itemUniqueId, map::Position{ packet.x ,packet.y });
	}
}

void ClientBot::handleLootRes(Session& session, const LootRes& packet)
{
	if (packet.result == 0)
	{
		InterlockedIncrement64(&mTotalRootItemCount);
	}
	session.ItemChaseInit();
}


void ClientBot::startInput(int32_t& userCnt)
{
userCnt_Input:
	std::cout << "시작 AccountNo 를 입력해주세요. : ";
	std::cin >> Session::g_accountNo;

	if (std::cin.fail())
	{
		std::string str;
		std::cin.clear();
		std::cin >> str;

		std::cout << "\n 숫자가 아닌 입력이 들어왔습니다. Input : " << str << "\n";
		goto userCnt_Input;
	}
	std::cout << "UserCnt 최대 : 1280 ,  최소 : 1\n";
	std::cout << "UserCnt 를 입력해주세요. : ";
	std::cin >> userCnt;

	if (std::cin.fail())
	{
		std::string str;
		std::cin.clear();
		std::cin >> str;

		std::cout << "\n 숫자가 아닌 입력이 들어왔습니다. Input : " << str << "\n";
		goto userCnt_Input;
	}
	if (1280 < userCnt || 0 >= userCnt)
	{
		std::cout << "\n범위를 벗어나는 숫자가 들어왔습니다. Input : " << userCnt << "\n";
		goto userCnt_Input;
	}

}

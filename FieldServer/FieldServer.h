#pragma once
#include "Contents/Player.h"
#include "Contents/Monster.h"
#include "Contents/FieldItem.h"
#include "Contents/DBJob.h"
#include "DropTable.h"
#include "Mapconfig.h"
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <map>
#include <cstdint>
#include <cpp_redis/cpp_redis>

#pragma comment(lib, "cpp_redis.lib")
#pragma comment(lib, "tacopie.lib")
#pragma comment(lib, "ws2_32.lib")

class CDB;   // _lib/CDB/framework.h — 참조/포인터 매개변수라 전방선언으로 충분

namespace contents
{
	enum Config
	{
		CONFIG_AUTH_FIELD_IDX = -1,
		CONFIG_UNAUTH_MAX_SIZE = 10000,
		CONFIG_AUTH_MAX_SIZE = 5000,
		CONFIG_UNAUTH_HEARTBEAT_TIMER = 30000,

		CONFIG_FIELD_SIZE = 1,
		CONFIG_MSG_SIZE = 3000,
		CONFIG_THREAD_MSGQ_SIZE = 3000,
		CONFIG_FRAME_INTERVAL = 20,

		CONFIG_NEAR_SECTOR_CNT = 9,
		CONFIG_DISCONNECT_AND_LEAVE_CAP = 1000,

		CONFIG_SINK_ABLE = 100,
		CONFIG_SINK_MAXCOUNT = 300,

		CONFIG_SESSION_SENDQ_CAPPACITY = 500,// player 송신 큐의 제한을 넘으면 DIsConnect

		CONFIG_REDIS_PORT = 6379,
	};
	constexpr float syncDistance = static_cast<float>(CONFIG_SINK_ABLE);
	constexpr char CONFIG_REDIS_IP[] = "127.0.0.1"; // IP는 문자열이라 enum에 못 넣음

	struct MonsterHitResult
	{
		int64_t monsterId;
		int32_t hp;
		float x;
		float y;
		int8_t direction;
	};

	class FieldServer final : public network::NetworkLib
	{
	public:
		FieldServer();
		~FieldServer();
		void Start();
	private:
		void createThread();

		void nearSectorInitalization();
		virtual void onAccept(SOCKADDR_IN& addr, const network::SeqAndIdx& sessionID) override;
		virtual void onRecv(utility::Message* msg) override;
		virtual void onSend(utility::Message* msg) override {}
		virtual void onRelease(const network::SeqAndIdx& sessionID) override;

		// ===================================
		//  auth 컨텐츠
		// ===================================
		void authThread();
		void heartBeatForUnAuthSession(int fieldIdx);
		void handleAuthReq(utility::Message& msg, CDB& db);
		void authPacketProc(utility::Message& msg, CDB& db);
		void sendAuthFail(const network::SeqAndIdx& sessionID);
		void enterField(const Player* player, int8_t targetField);
		// ===================================
		//  Field 컨텐츠
		// ===================================
		void fieldThread(int fieldIdx);
		void fieldUpdate(int fieldIdx);
		void playerProc(int fieldidx , DWORD startTime );
		void monsterProc();
		void spawnMonsters(int fieldIdx);
		bool possibleChasePlayer(int64_t playerID , map::Position& targetPos);
		void processMonsterAttackHit(Monster& monster);
		void processPlayerAttackHit(Player& player, int fieldIdx);
		// ===================================
		//  DB 컨텐츠
		// ===================================
		void dbThread(int fieldIdx);
		void requestPositionSave(int fieldIdx, Player& player);
		void requestKillCountUpdate(int fieldIdx, Player& player, int64_t killCnt);
		void requestRankingQuery(int fieldIdx, Player& player);
		void dbResultPacketProc(int fieldIdx);
		// ===================================
		// Thread간의 PacketProc
		// ===================================
		void fieldPacketProc(int fieldIdx, utility::Message& msg);
		void handleRegisterPlayer(int fieldIdx, utility::Message& msg);
		// ===================================
		// Session간의 PacketPoce
		// ===================================
		void fieldContentsPacketProc(utility::Message& msg);

		void handleMoveStart(utility::Message& msg, int64_t seqID);
		void handleMoveStop(utility::Message& msg, int64_t seqID);
		void handleAttackReq(utility::Message& msg, int64_t seqID);

		void broadcastMoveStart(const Player& sendPlayer, const std::vector<map::Sector>& nearSector);
		void broadcastMoveStop(const Player& sendPlayer, const std::vector<map::Sector>& nearSector);
		void broadcastPlayerAttack(const Player& sendPlayer, const std::vector<map::Sector>& nearSector);

		void broadcastMonsterMoveStart(const Monster& monster, const std::vector<map::Sector>& nearSector);
		void broadcastMonsterMoveStop(const Monster& monster, const std::vector<map::Sector>& nearSector);
		void broadcastMonsterAttack(const Monster& monster, const std::vector<map::Sector>& nearSector);

		void broadcastMonsterSpawn(const Monster& monster, const std::vector<map::Sector>& nearSector);
		void broadcastMonsterDespawn(const Monster& monster, const std::vector<map::Sector>& nearSector);
		void broadcastMonsterDamaged(const Monster& monster, const std::vector<map::Sector>& nearSector);
		void broadcastCharacterDamaged(const Player& damagedPlayer, const Monster& attacker, const std::vector<map::Sector>& nearSector);

		void checkDisConnectedAndLeavePlayer(int fieldIdx);
		void notifyDisconnect(int fieldIdx, int64_t accountNo);
		void notifyMoveField(int fieldIdx, int64_t accountNo, int32_t targetFieldIdx);
		void authNotifyPacketProc(utility::Message& msg);
		// ===================================
		// Sector관련 함수
		// ===================================

		void calcSector(const map::Position& pos, __out map::Sector& newSector);
		void enterSector(Player& player);

		void updateSectorChechk(Player& player);
		void updateMonsterSectorCheck(Monster& monster);
		void removeFromSector(Player& player);

		void getAroundSectors(const map::Sector& center, __out std::vector<map::Sector>& out) const;
		void calcDespawnAndSpawnSector(const Player& player, __out std::vector<map::Sector>& spawnSector, __out std::vector<map::Sector>& despawnSector)const;
		void calcMonsterDespawnAndSpawnSector(const Monster& monster, __out std::vector<map::Sector>& spawnSector, __out std::vector<map::Sector>& despawnSector)const;

		void broadcastSpawn(const Player& sendPlayer, const std::vector<map::Sector>& nearSector);
		void sendSpawnBatch(const Player& targetPlayer, const std::vector<map::Sector>& nearSector);
		void broadcastDespawn(const Player& targetPlayer, const std::vector<map::Sector>& nearSector);
		void sendDespawnBatch(const Player& targetPlayer, const std::vector<map::Sector>& nearSector);
		void sendMonsterSpawnBatch(const Player& targetPlayer, const std::vector<map::Sector>& nearSector);
		void sendMonsterDespawnBatch(const Player& targetPlayer, const std::vector<map::Sector>& nearSector);

		void dropItem(const Monster& monster);
		void broadcastItemSpawn(const FieldItem& item, const std::vector<map::Sector>& nearSector);
		void broadcastItemDespawn(const FieldItem& item, const std::vector<map::Sector>& nearSector);
		void sendItemSpawnBatch(const Player& targetPlayer, const std::vector<map::Sector>& nearSector);
		void sendItemDespawnBatch(const Player& targetPlayer, const std::vector<map::Sector>& nearSector);
		void handleLootReq(utility::Message& msg, int64_t seqID);
		void sendLootRes(const Player& targetPlayer, bool bSuccess, eItemId itemId, int32_t count);
		void handleRankingReq(utility::Message& msg, int64_t seqID);
		// ===================================
		// Monitor관련 함수
		// ===================================
		void monitorThread() const ;
		friend std::ostream& operator >> (std::ostream& out, const FieldServer& server);
	private:
		volatile LONG64 mbOn;
		volatile LONG64 mCharacterID;
		volatile LONG64 mItemUniqueID;
		// ===================================
		//  auth 컨텐츠
		// ===================================
		std::thread mAuthThread;
		HANDLE hAuthEvent;
		utility::MyRingBuffer* mNotifyMsgQ[CONFIG_FIELD_SIZE];
		// AuthThread의 동기화 객체

		std::shared_mutex mUnAuthLock;
		std::unordered_map<int64_t, Player*> mUnAuthSessions;
		std::unordered_map<int64_t, Player*> mAccountsHash;
		// ===================================
		//  Field 컨텐츠
		// ===================================
		std::thread mFieldThread[CONFIG_FIELD_SIZE];
		// Field마다 메세지 큐 존재.
		std::shared_mutex mThreadLock[CONFIG_FIELD_SIZE];
		utility::MyRingBuffer* mMsgQ[CONFIG_FIELD_SIZE];

		// ===================================
		//  DB 컨텐츠 (dbThread, 필드당 1:1 SPSC라 락 없음)
		// ===================================
		std::thread mDBThread[CONFIG_FIELD_SIZE];
		HANDLE hDBEvent[CONFIG_FIELD_SIZE];
		HANDLE hDBFinishEvent[CONFIG_FIELD_SIZE]; // 수동이벤트

		utility::MyRingBuffer* mDBReqQ[CONFIG_FIELD_SIZE];   // fieldThread -> dbThread
		utility::MyRingBuffer* mDBResQ[CONFIG_FIELD_SIZE];   // dbThread -> fieldThread

		// ===================================
		//  Sector
		// ===================================
		std::unordered_map<int64_t, std::vector<int>> mCacheNearSectorInfo;
		std::map<map::Sector, std::vector<map::Sector>> mAroundSectorCache;
		// ===================================
		// Monitoring Data
		// ===================================
		std::thread mMonitorThread;
		bool bMonitorOn;
		int64_t mPlayerCnt;
		int64_t mDisconnect_Sync;
		int64_t mDisconnect_HeartBeat;
		int64_t mDisconnect_SendQisFull;

		int64_t mAuthMessageQCnt;
		int64_t mFieldMessageQCnt[CONFIG_FIELD_SIZE];
		int64_t mDBMessageQCnt[CONFIG_FIELD_SIZE]; // dbThread별 아직 처리 못한 mDBReqQ 잔여 개수
		int64_t mPlayerMessageQCnt;
		uint64_t mMessageDeQTPS;
		uint64_t mMessageInQTPS;
		uint64_t mProcessDelaySum;

		volatile int64_t mTotalRootItemCount;
		DWORD mFrameTime;
	};
}


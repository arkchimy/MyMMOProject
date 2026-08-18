#pragma once
#include <cstdint>
enum class eDirection : int8_t
{
	Down = 0,
	DownLeft,
	DownRight,

	Left,
	Right,

	Up,
	UpLeft,
	UpRight,
	Max,
};

struct DirectionVector
{
	float mX;
	float mY;
};

inline static const DirectionVector kDirectionVectorTable[static_cast< int8_t >(eDirection::Max)] =
{
	{ 0.0f,  1.0f },  // Down
	{ -0.707107f,  0.707107f },  // DownLeft
	{ 0.707107f,  0.707107f },  // DownRight
	{ -1.0f,  0.0f },  // Left
	{ 1.0f,  0.0f },  // Right
	{ 0.0f, -1.0f },  // Up
	{ -0.707107f, -0.707107f },  // UpLeft
	{ 0.707107f, -0.707107f },  // UpRight
};

enum class eItemId : int8_t
{
	Gold,
	Potion,
	Max,
};

enum class PacketType : int16_t
{
	// ==============================
	// LoginServer
	// ==============================
	//  {
	//      int16_t type;
	//      char    id[20];
	//      char    pw[20];
	//  }
	LOGIN_REQ = 0,


	//  {
	//      int16_t type;
	//      int8_t  result;
	//      char    fieldIp[16];
	//      int16_t fieldPort;
	//  }
	LOGIN_RES,


	// ==============================
	// FieldServer   [ Inner ]
	// ==============================
	//  {
	//      int16_t type;
	//      Player* ptr;
	//  }
	REGISTER_PLAYER = 50,		// 추가: authThread →fieldThread 알림
	//  {
	//      int16_t type;
	//      int64_t accountNo;
	//  }
	PLAYER_DISCONNECTED = 51,   // 추가: fieldThread →authThread 알림
	//  {
	//      int16_t type;
	//      int64_t accountNo;
	//		int32_t targetField; 
	//  }
	PLAYER_MOVEFILED = 52,   // 추가: fieldThread →authThread 알림
	
	// ==============================
	// FieldServer
	// ==============================

	//  {
	//      int16_t type;
	//      char    id[32];
	//      char    pw[64];
	//  }
	FIELD_AUTH_REQ = 100,
	//  {
	//		int16_t type;
	//		int8_t  result;
	//  }
	FIELD_AUTH_FAIL,
	//  {
	//      int16_t type;
	//		int64_t characterID;
	//      float   x;  // 서버가 정한 스폰 좌표
	//      float   y;
	//  }
	FIELD_AUTH_RES,
	//  {
	//      int16_t type;
	//      int64_t characterId;
	//      float   x;
	//      float   y;
	//		int8_t  state;      // Idle/Move/Attack/Dead
	//		int8_t  direction;
	//		float   speed
	//		int32_t animFrame;
	//      int8_t  characterType;
	//      char    nickname[20];
	//  }
	OTHER_CHARACTER_SPAWN,
	//  {
	//      int16_t type;
	//      int16_t cnt;
	//      // cnt만큼 반복:
	//      int64_t characterId;
	//      float   x;
	//      float   y;
	// 		int8_t  state;      // Idle/Move/Attack/Dead
	//		int8_t  direction;
	//		float   speed
	//		int32_t animFrame;
	//      int8_t  characterType;
	//      char    nickname[20];
	//  }
	OTHER_CHARACTER_SPAWN_BATCH,
	//  {
	//      int16_t type;
	//      int64_t characterId;
	//  }
	CHARACTER_DESPAWN,
	//  {
	//      int16_t type;
	//      int64_t characterId;
	//      float   x;
	//      float   y;
	//      int8_t  direction;
	//		float   speed;
	//  }
	MOVE_START,


	//  {
	//      int16_t type;
	//      int64_t characterId;
	//      float   x;
	//      float   y;
	//      int8_t  direction;
	//  }
	MOVE_STOP,

	//  {
	//      int16_t type;
	//      int64_t monsterId;
	//      float   x;
	//      float   y;
	// 		int8_t  mState;
	//      int8_t  direction;
	//      int32_t hp;
	//      int8_t  monsterType;
	//      int32_t animFrame;   // 현재 상태 진입 후 경과 틱수
	//  }
	MONSTER_SPAWN,

	//  {
	//      int16_t type;
	//      int16_t cnt;
	//      // cnt만큼 반복:
	//      int64_t monsterId;
	//      float   x;
	//      float   y;
	// 		int8_t  mState;
	//      int8_t  direction;
	//      int32_t hp;
	//      int8_t  monsterType;
	//      int32_t animFrame;   // 현재 상태 진입 후 경과 틱수
	//  }
	MONSTER_SPAWN_BATCH,

	//  {
	//      int16_t type;
	//      int64_t monsterId;
	//  }
	MONSTER_DESPAWN,

	//  {
	//      int16_t type;
	//      int64_t monsterId;
	//      float   x;
	//      float   y;
	//      int8_t  direction;
	//      float   speed;
	//  }
	MONSTER_MOVE_START,

	//  {
	//      int16_t type;
	//      int64_t monsterId;
	//      float   x;
	//      float   y;
	//      int8_t  direction;
	//  }
	MONSTER_MOVE_STOP,

	//  {
	//      int16_t type;
	//      int8_t  skillId;      // 0 = 기본 공격
	//      int16_t targetCnt;
	//      // targetCnt만큼 반복:
	//      int64_t monsterId;
	//  }
	PLAYER_ATTACK_REQ,

	//  {
	//      int16_t type;
	//      int64_t characterId;
	//  }
	OTHER_CHARACTER_ATTACK,

	//  {
	//      int16_t type;
	//      int64_t monsterId;
	//      int32_t hp;
	//      float   x;
	//      float   y;
	//      int8_t  direction;
	//  }
	MONSTER_DAMAGED,

	//  {
	//      int16_t type;
	//      int64_t monsterId;
	//  }
	MONSTER_ATTACK,          // 몬스터 공격 시작(윈드업) 브로드캐스트 — OTHER_CHARACTER_ATTACK과 동일 패턴

	//  {
	//      int16_t type;
	//      int64_t characterId;
	//      int32_t hp;
	//      float   x;
	//      float   y;
	//      int8_t  direction;
	//      int64_t monsterId;   // 누구한테 맞았는지 (반격 대상 판단용)
	//  }
	CHARACTER_DAMAGED,       // 몬스터에게 맞은 플레이어 데미지/넉백 브로드캐스트 — MONSTER_DAMAGED와 대칭(단일 대상, batch 아님)

	//  {
	//      int16_t type;
	//      int64_t itemUniqueId;
	//      int8_t  itemId;
	//      float   x;
	//      float   y;
	//  }
	ITEM_SPAWN,          // 몬스터 사망 등으로 필드에 아이템 생성 — 주변 섹터 브로드캐스트

	//  {
	//      int16_t type;
	//      int16_t cnt;
	//      // cnt만큼 반복:
	//      int64_t itemUniqueId;
	//      int8_t  itemId;
	//      float   x;
	//      float   y;
	//  }
	ITEM_SPAWN_BATCH,    // 신규 입장/섹터 진입 시 기존 아이템 목록

	//  {
	//      int16_t type;
	//      int64_t itemUniqueId;
	//  }
	ITEM_DESPAWN,        // 루팅되었거나 AOI 이탈

	//  {
	//      int16_t type;
	//      int64_t itemUniqueId;
	//  }
	LOOT_REQ,            // 클라이언트 → 서버, F키

	//  {
	//      int16_t type;
	//      int8_t  result;      // 0=성공, 1=실패(이미 없어짐/거리초과)
	//      int8_t  itemId;
	//      int32_t count;
	//  }
	LOOT_RES,            // 서버 → 요청자 1인 (성공/실패 항상 응답)

	//  {
	//      int16_t type;
	//  }
	RANKING_REQ,         // 클라이언트 → 서버, 랭킹 조회 요청 (별도 페이로드 없음, 세션으로 본인 식별)

	//  {
	//      int16_t type;
	//      int16_t topCnt;         // TOP 목록 개수 (최대 200)
	//      // topCnt만큼 반복:
	//      char    nickname[20];
	//      int64_t killCount;
	//      int64_t myRank;         // 본인 순위 (1-based). 아직 킬이 없으면 -1
	//      int64_t myKillCount;    // 본인 킬카운트
	//  }
	RANKING_RES,

	MAX,
};
#pragma once
#include <cstdint>
#include "../_Shared/Protocol.h"

#pragma pack(1)
//서버(_lib/AcceptEx_IOCP_NetworkLib/Header.h)와 동일한 와이어 포맷

struct Header
{
	int16_t Len;		//페이로드 길이 (헤더 3바이트 제외)
	int8_t RandKey;		//미사용, 0 고정 (서버 EnCoding이 빈 함수)
};

struct FieldAuthReqPacket
{
	Header header;
	int16_t type;		//PacketType::FIELD_AUTH_REQ
	char id[20];		//널문자 포함 20바이트 — 서버 id[20]/클라 mID[20]과 크기 일치. mAccountNo에서 파생 ("bot"+숫자), 영문/숫자만(서버 화이트리스트). 재접속해도 동일
	char pw[64];		//봇 전용 고정값 (보안 불필요)
};
//FIELD_AUTH_REQ : 접속 직후 인증 요청

struct FieldAuthResPayload
{
	int16_t type;
	int64_t characterId;
	float x;
	float y;
};
//FIELD_AUTH_RES : 인증 성공 응답 (페이로드만, 조립부가 헤더를 제거하고 넘겨줌)

struct MoveStartPacket
{
	Header header;
	int16_t type;
	int64_t characterId;
	float x;
	float y;
	int8_t direction;
	float speed;	//서버 Player::mSpeed와 정확히 일치해야 함 (RT_ASSERT 검증 대상)
};
//MOVE_START : 이동 시작 알림

struct MoveStopPacket
{
	Header header;
	int16_t type;
	int64_t characterId;
	float x;
	float y;
	int8_t direction;
};
//MOVE_STOP : 이동 정지 알림 (송신용, Header 포함)

struct MoveStopPayload
{
	int16_t type;
	int64_t characterId;
	float x;
	float y;
	int8_t direction;
};
//MOVE_STOP / MONSTER_MOVE_STOP 수신 파싱용 (Header 제외 — packetProc이 이미 header 건너뛴 포인터+길이를 넘겨줌)

struct MonsterMoveStop
{
	Header header;
	int16_t type;
	int64_t monsterId;
	float   x;
	float   y;
	int8_t  direction;
};
//MONSTER_MOVE_STOP,

struct PlayerAttackReq
{
	Header header;
	int16_t type;
	int8_t  skillId;      // 0 = 기본 공격
	int16_t targetCnt;
	// targetCnt만큼 반복:
	int64_t monsterId;
};
//PLAYER_ATTACK_REQ

struct MonsterDamaged
{
	int16_t type;
	int64_t monsterId;
	int32_t hp;
	float   x;
	float   y;
	int8_t  direction;
};
//MONSTER_DAMAGED,
// 
struct CharacterDamaged
{
	int16_t type;
	int64_t characterId;
	int32_t hp;
	float   x;
	float   y;
	int8_t  direction;
	int64_t monsterId;   // 누구한테 맞았는지 (반격 대상 판단용)
};
//CHARACTER_DAMAGED,       // 몬스터에게 맞은 플레이어 데미지/넉백 브로드캐스트 — MONSTER_DAMAGED와 대칭(단일 대상, batch 아님)
struct ItemSpawn
{
	int16_t type;
	int64_t itemUniqueId;
	int8_t  itemId;
	float   x;
	float   y;
};
//ITEM_SPAWN,          // 몬스터 사망 등으로 필드에 아이템 생성 — 주변 섹터 브로드캐스트
struct ItemDeSpawn
{
	Header header;
	int16_t type;
	int64_t itemUniqueId;
};
//ITEM_DESPAWN,        // 루팅되었거나 AOI 이탈
struct LootReq
{
	Header header;
	int16_t type;
	int64_t itemUniqueId;
};
//LOOT_REQ,            // 클라이언트 → 서버, F키

struct LootRes
{
	int16_t type;
	int8_t  result;      // 0=성공, 1=실패(이미 없어짐/거리초과)
	int8_t  itemId;
	int32_t count;
};
//LOOT_RES,            // 서버 → 요청자 1인 (성공/실패 항상 응답)

#pragma pack()

#pragma once
#include <cstdint>

// dbThread와 fieldThread가 주고받는 내부 전용 쪽지.
// mDBReqQ / mDBResQ(필드당 1개, dbThread와 1:1 SPSC) 위를 raw bytes로 그대로 오간다.
// 큐 하나를 여러 job 종류가 공유하므로, jobType이 "지금 꺼낸 게 뭔지" 알려주는 태그 역할을 한다.
namespace contents
{
	enum class eDBJobType : int8_t
	{
		PositionSave = 1,
		KillCountUpdate = 2,
		RankingQuery = 3,
		MAX,
	};

	// Player::mDBRequestCnt 최상위 비트: disconnect 저장 요청을 보내고 응답 대기중임을 표시
	constexpr uint64_t DB_POSITION_SAVE_PENDING_BIT = 1ULL << 63;

	// fieldThread -> dbThread : "이거 해줘" 요청
	class DBJob
	{
	public:
		DBJob(const eDBJobType jobType, const int64_t SeqID, const int64_t AccountNo)
			: mJobType(jobType)
			, mTypeID(static_cast<int8_t>(jobType))
			, mSeqID(SeqID)
			, mAccountNo(AccountNo) {
		}
		virtual ~DBJob() {};

		DBJob(const DBJob& other) = delete;
		DBJob(DBJob&& rhs) = delete;

		int8_t GetTypeID() const { return mTypeID; }
		int64_t GetSeqID() const { return mSeqID; }
		int64_t GetAccount() const { return mAccountNo; }

		DBJob& operator =(const DBJob& rhs) = delete;
		DBJob& operator =(DBJob&& rhs) = delete;
	protected:
		eDBJobType mJobType;
		int64_t    mSeqID;      // 응답 왔을 때 players.find(seqID)로 대상을 다시 찾기 위한 열쇠
		int64_t    mAccountNo;  // DB WHERE절에 쓸 열쇠
	private:
		int8_t mTypeID;
	};

	class DBPositionSave final : public DBJob
	{
	public:
		DBPositionSave(const int64_t SeqID, const int64_t AccountNo, const map::Position& pos)
			:DBJob(eDBJobType::PositionSave, SeqID, AccountNo)
			, mPos(pos)
		{}
		const map::Position& GetPos() const { return mPos; }
	private:
		map::Position mPos;

	};

	class DBKillCountUpdate final : public DBJob
	{
	public:
		DBKillCountUpdate(const int64_t SeqID, const int64_t AccountNo, const char* nickname, int64_t cnt)
			:DBJob(eDBJobType::KillCountUpdate, SeqID, AccountNo)
			, mNickname{0}
			, mCnt(cnt)
		{
			memcpy(mNickname, nickname, sizeof(mNickname));
		}
		const char* const GetNickName() const { return mNickname; }
		int64_t GetKiilCnt() const { return mCnt; }
	private:
		char mNickname[20];
		int64_t mCnt;
	};

	class DBRankingQuery final : public DBJob
	{
	public:
		DBRankingQuery(const int64_t SeqID, const int64_t AccountNo, const char* nickname)
			:DBJob(eDBJobType::RankingQuery, SeqID, AccountNo)
			, mNickname{0}
		{
			memcpy(mNickname, nickname, sizeof(mNickname));
		}
		const char* const GetNickName() const { return mNickname; }
	private:
		char mNickname[20];
	};

	// dbThread -> fieldThread : "다 했어" 응답
	// DBJob과 동일하게 다형성 베이스 — 결과가 커지는 잡(랭킹 조회 등)은
	// 서브클래스로 필요한 만큼 데이터를 더 들고, 포인터로 큐를 오간다.
	class DBResult
	{
	public:
		DBResult(const eDBJobType jobType, const int64_t seqID, const bool bSuccess)
			: mJobType(jobType)
			, mTypeID(static_cast<int8_t>(jobType))
			, mSeqID(seqID)
			, mSuccess(bSuccess) {
		}
		virtual ~DBResult() {};

		DBResult(const DBResult& other) = delete;
		DBResult(DBResult&& rhs) = delete;

		int8_t GetTypeID() const { return mTypeID; }
		int64_t GetSeqID() const { return mSeqID; }
		bool IsSuccess() const { return mSuccess; }

		DBResult& operator =(const DBResult& rhs) = delete;
		DBResult& operator =(DBResult&& rhs) = delete;
	protected:
		eDBJobType mJobType;
		int64_t    mSeqID;
		bool       mSuccess;
	private:
		int8_t mTypeID;
	};

	struct RankEntry
	{
		char    nickname[20];
		int64_t killCount;
	};

	class DBRankingResult final : public DBResult
	{
	public:
		enum : int32_t
		{
			MAX_TOP_ENTRY = 200,
		};

		DBRankingResult(const int64_t seqID, const bool bSuccess, const int32_t topCnt, const int64_t myRank, const int64_t myKillCount)
			: DBResult(eDBJobType::RankingQuery, seqID, bSuccess)
			, mTopCnt(topCnt)
			, mMyRank(myRank)
			, mMyKillCount(myKillCount)
			, mTopEntries{}
		{
		}

		int32_t GetTopCnt() const { return mTopCnt; }
		int64_t GetMyRank() const { return mMyRank; }
		int64_t GetMyKillCount() const { return mMyKillCount; }
		RankEntry& GetEntry(int32_t idx) { return mTopEntries[idx]; }
		const RankEntry& GetEntry(int32_t idx) const { return mTopEntries[idx]; }

	private:
		int32_t   mTopCnt;
		int64_t   mMyRank;
		int64_t   mMyKillCount;
		RankEntry mTopEntries[MAX_TOP_ENTRY];
	};
}

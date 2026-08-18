#pragma once
#include <cstdint>

// 사용법 : 항상 DeQ,EnQ,Peek는 성공조건을 확인 후 호출한다.
// 만일 Fail을 하였다면,Clear를 하지않으면 동작하지않는다. 
namespace utility
{
	enum class eRingBufferFailBit : uint8_t
	{
		None = 0,
		BufferFull = 1,   // 빈 공간 부족
		BufferEmpty = 2,  // 데이터가 Size만큼 없음.
		BufferPeekEmpty =3,  // Peek 에서데이터가 Size만큼 없음.
		ArgmentSizeInvalid ,// size로 음수를 등록
	};
	class MyRingBuffer final
	{
	public:
		MyRingBuffer();
		MyRingBuffer(const MyRingBuffer&) = delete;
		MyRingBuffer(MyRingBuffer&&) = delete;

		MyRingBuffer& operator=(const MyRingBuffer&) = delete;
		MyRingBuffer& operator=(MyRingBuffer&&) = delete;

		enum eConfig
		{
			RINGBUFFER_SIZE = 30000
		};
	public:
		void Peek(void* dest, const int32_t size);
		void Enqueue(const void* Src, const int32_t size);
		void Dequeue(void* Dest, const int32_t size);

		int32_t GetDirectFreeSize(const char* f, const char* r) const;
		int32_t GetDirectUseSize(const char* f, const char* r) const;

		int32_t GetFreeSize(const char* f, const char* r) const;
		int32_t GetUseSize(const char* f, const char* r) const;

		void MoveFront(const int32_t move);
		void MoveRear(const int32_t move);

		inline uint8_t IsFail() const { return bFail; }
		void ClearBuffer();

		inline char* GetFrontPtr() const { return mFront; }
		inline char* GetRearPtr() const { return mRear; }
		inline char* GetBeginPtr() const { return mBegin; }

	private:
		char mBuffer[eConfig::RINGBUFFER_SIZE];
		char* const mBegin;
		char* const mEnd;
		char* mFront;
		char* mRear;

		uint8_t bFail;
	};
}


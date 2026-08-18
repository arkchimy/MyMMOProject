#include "MyRingBuffer.h"
#include <cassert>
#include <iostream>
#include <iomanip>

#define RINGBUFFER_ASSERT() \
do{\
	if (bFail)\
	{\
		if(bFail & (1 << static_cast<uint8_t>(eRingBufferFailBit::BufferFull)))\
		{\
			std::cout << std::setw(10) << "BufferFull\n";\
		}\
		if(bFail & (1 << static_cast<uint8_t>(eRingBufferFailBit::BufferEmpty)))\
		{\
			std::cout << std::setw(10) << "BufferEmpty\n";\
		}\
		if(bFail & (1 << static_cast<uint8_t>(eRingBufferFailBit::ArgmentSizeInvalid)))\
		{\
			std::cout << std::setw(10) << "ArgmentSizeInvalid\n";\
		}\
		__debugbreak();\
	}\
}while(0)
namespace utility
{
	MyRingBuffer::MyRingBuffer()
		:mBuffer{ 0 }
		, mBegin(mBuffer)
		, mEnd(mBuffer + eConfig::RINGBUFFER_SIZE)
		, mFront(mBuffer)
		, mRear(mBuffer)
		, bFail(false)
	{
	}
	void MyRingBuffer::Peek(void* Dest, const int32_t size)
	{
		RINGBUFFER_ASSERT();
		if (size < 0)
		{
			bFail |= 1 << static_cast<uint8_t>(eRingBufferFailBit::ArgmentSizeInvalid);
			return;
		}
		char* f = mFront;
		char* r = mRear;

		char* dest = static_cast<char*>(Dest);

		int32_t useSize = GetUseSize(f, r);
		int32_t directUseSize = GetDirectUseSize(f, r);

		if (useSize < size)
		{
			bFail |= 1 << static_cast<uint8_t>(eRingBufferFailBit::BufferEmpty);
			return;
		}

		if (size <= directUseSize)
		{
			for (int32_t cnt = 0; cnt < size; ++cnt)
			{
				*(dest++) = *(f++);
			}
		}
		else
		{
			for (int32_t cnt = 0; cnt < directUseSize; ++cnt)
			{
				*(dest++) = *(f++);
			}
			f = mBegin;
			for (int32_t cnt = 0; cnt < size - directUseSize; ++cnt)
			{
				*(dest++) = *(f++);
			}

		}
	}

	void MyRingBuffer::Enqueue(const void* Src, const int32_t size)
	{
		RINGBUFFER_ASSERT();
		if (size < 0)
		{
			bFail |= 1 << static_cast<uint8_t>(eRingBufferFailBit::ArgmentSizeInvalid);
			return;
		}

		const char* src = static_cast<const char*>(Src);

		char* f = mFront;
		char* r = mRear;
		int32_t freeSize = GetFreeSize(f, r);
		if (freeSize < size)
		{
			bFail |= 1 << static_cast<uint8_t>(eRingBufferFailBit::BufferFull);
			return;
		}
		int32_t directFreeSize = GetDirectFreeSize(f, r);
		if (size <= directFreeSize)
		{
			for (int32_t cnt = 0; cnt < size; ++cnt)
			{
				*(r++) = *(src++);
			}
		}
		else
		{
			for (int32_t cnt = 0; cnt < directFreeSize; ++cnt)
			{
				*(r++) = *(src++);
			}
			r = mBegin;
			for (int32_t cnt = 0; cnt < size - directFreeSize; ++cnt)
			{
				*(r++) = *(src++);
			}

		}
		MoveRear(size);

	}

	void MyRingBuffer::Dequeue(void* Dest, const int32_t size)
	{
		RINGBUFFER_ASSERT();
		if (size < 0)
		{
			bFail |= 1 << static_cast<uint8_t>(eRingBufferFailBit::ArgmentSizeInvalid);
			return;
		}
		char* dest = static_cast<char*>(Dest);

		char* f = mFront;
		char* r = mRear;
		int32_t useSize = GetUseSize(f, r);
		if (useSize < size)
		{
			bFail |= 1 << static_cast<uint8_t>(eRingBufferFailBit::BufferEmpty);
			return;
		}
		int32_t directUseSize = GetDirectUseSize(f, r);

		if (size <= directUseSize)
		{
			for (int32_t cnt = 0; cnt < size; ++cnt)
			{
				*(dest++) = *(f++);
			}
		}
		else
		{
			for (int32_t cnt = 0; cnt < directUseSize; ++cnt)
			{
				*(dest++) = *(f++);
			}
			f = mBegin;
			for (int32_t cnt = 0; cnt < size - directUseSize; ++cnt)
			{
				*(dest++) = *(f++);
			}

		}
		MoveFront(size);
	}

	void MyRingBuffer::ClearBuffer()
	{
		mFront = mBegin;
		mRear = mBegin;
		bFail = 0;
	}

	void MyRingBuffer::MoveFront(int32_t move)
	{
		if (IsFail())
		{
			return;
		}
		if (move < 0)
		{
			bFail |= 1 << static_cast<uint8_t>(eRingBufferFailBit::ArgmentSizeInvalid);
			return;
		}
		char* f = mFront;
		f += move;
		if (mEnd <= f)
		{
			size_t distance = reinterpret_cast<size_t>(f) - reinterpret_cast<size_t>(mEnd);
			mFront = mBegin + distance;
			return;
		}
		mFront = f;
	}
	void MyRingBuffer::MoveRear(int32_t move)
	{
		RINGBUFFER_ASSERT();
		if (move < 0)
		{
			bFail |= 1 << static_cast<uint8_t>(eRingBufferFailBit::ArgmentSizeInvalid);
			return;
		}
		char* r = mRear;
		r += move;
		if (mEnd <= r)
		{
			size_t distance = reinterpret_cast<size_t>(r) - reinterpret_cast<size_t>(mEnd);
			mRear = mBegin + distance;
			return;
		}
		mRear = r;
	}

	int32_t MyRingBuffer::GetDirectFreeSize(const char* f, const char* r) const
	{
		if (f <= r)
		{
			if (f == mBegin)
			{
				return static_cast<int32_t>(mEnd - r - 1);
			}
			return static_cast<int32_t>(mEnd - r);
		}
		return static_cast<int32_t>(f - r - 1);
	}

	int32_t MyRingBuffer::GetDirectUseSize(const char* f, const char* r) const
	{
		if (f <= r)
		{
			return static_cast<int32_t>(r - f);
		}
		size_t distance = mEnd - f;
		return static_cast<int32_t>(distance);
	}

	int32_t MyRingBuffer::GetFreeSize(const char* f, const char* r) const
	{
		volatile size_t distance = 0;
		// 같은 결과
		/*if (f == r)
		{
			return static_cast<int32_t>(mEnd - mBegin - 1);
		}*/
		if (f <= r)
		{
			distance = mEnd - r;
			distance += f - mBegin - 1;
			return static_cast<int32_t>(distance);
		}
		distance = f - r - 1;
		return static_cast<int32_t>(distance);
	}
	int32_t MyRingBuffer::GetUseSize(const char* f, const char* r) const
	{
		size_t distance = 0;
		if (f <= r)
		{
			distance = reinterpret_cast<size_t>(r) - reinterpret_cast<size_t>(f);
			return static_cast<int32_t>(distance);
		}
		distance = reinterpret_cast<size_t>(mEnd) - reinterpret_cast<size_t>(f);
		distance += reinterpret_cast<size_t>(r) - reinterpret_cast<size_t>(mBegin);
		return static_cast<int32_t>(distance);
	}

};

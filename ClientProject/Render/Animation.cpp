#include "Animation.h"
#include "Image.h"
#include "ImageManager.h"
#include "utility/Common.h"

namespace render
{
	Animation::Animation(const eAnimationType type, const int& speed)
		: mCurrentFrame(0)
		, mType(type)
		, mSpeed(speed)
		, mSpeedCnt(0)
		, mFilename(nullptr)
		, mAtlasImage(nullptr)
		, mFrameCount(0)
		, mCols(0)
		, mFrameW(0)
		, mFrameH(0)
		, mStartRow(0)
		, mSRV(nullptr)
	{
	}

	Animation::~Animation()
	{
		if (mFilename != nullptr)
		{
			ImageManager::GetInstance()->Release(mFilename);
		}
	}
	void Animation::AddImage(const char* const filename)
	{
		//mFilename = filename;
		//const Image* img = ImageManager::GetInstance()->Load(filename);
		//vec.emplace_back(img);
	}
	void Animation::SetAtlas(const char* filename, const int frameCount, const int cols, const int frameW, const int frameH, const int startRow)
	{
		mFilename = filename;
		mAtlasImage = ImageManager::GetInstance()->Load(filename);
		// 아틀라스 전체를 가져옴.

		mStartRow = startRow;
		mFrameCount = frameCount;
		mCols = cols;
		mFrameW = frameW;
		mFrameH = frameH;
		mSRV = mAtlasImage->GetSRV();
	}
	bool Animation::Run()
	{
		// Vec.size 가 3일때 mCurrentFrame는 2가 마지막
		// Actor::Update에서 Run을 호출하면 0부터 시작해야하므로 
		// SRV를 초기화한 후 Run을 호출해야함.

		if (mCurrentFrame != mFrameCount)
		{
			++mSpeedCnt;
			if (mSpeedCnt == mSpeed)
			{
				++mCurrentFrame;
				mSpeedCnt = 0;
				if (mCurrentFrame == mFrameCount)
				{
					if (mType != eAnimationType::EndStop)
					{
						mCurrentFrame = 0;
					}
					else
					{
						mCurrentFrame = mFrameCount - 1;
					}
				}
				return true;
			}
		}
		return false;
	}
	void Animation::FastForward(const int ticks)
	{
		RT_ASSERT(mFrameCount != 0);

		const int frame = ticks / mSpeed;
		if (mType == eAnimationType::EndStop)
		{
			mCurrentFrame = (frame < mFrameCount) ? frame : mFrameCount - 1;
		}
		else
		{
			mCurrentFrame = frame % mFrameCount;
		}
		mSpeedCnt = 0;
	}
	void Animation::Initialize()
	{
		mCurrentFrame = 0;
		mSpeedCnt = 0;
	}
	ID3D11ShaderResourceView* Animation::GetCurrentSRV() const
	{
		RT_ASSERT(0 <= mCurrentFrame);
		RT_ASSERT(mSRV != nullptr);
		return mSRV;
	}
	ID3D11SamplerState* Animation::GetCurrentSampleState() const
	{
		RT_ASSERT(0 <= mCurrentFrame);
		return mAtlasImage->GetSampleState();
	}
	void Animation::GetCurrentUVOffset(float& uvX, float& uvY)
	{
		float row = static_cast<float>((mCurrentFrame + mStartRow) / mCols);
		float col = static_cast<float>((mCurrentFrame + mStartRow) % mCols);
		// 64 x64 의 경우 0이면 0 ,0을 반환?
		RT_ASSERT(mAtlasImage->mWidth != 0);
		RT_ASSERT(mAtlasImage->mHeight != 0);
		uvX = col * mFrameW / mAtlasImage->mWidth;
		uvY = row * mFrameH / mAtlasImage->mHeight;
	}
	void Animation::GetCurrentUVScale(float& scaleX, float& scaleY)
	{
		RT_ASSERT(mAtlasImage->mWidth != 0);
		RT_ASSERT(mAtlasImage->mHeight != 0);
		scaleX = (float)mFrameW / mAtlasImage->mWidth;
		scaleY = (float)mFrameH / mAtlasImage->mHeight;
	}
};
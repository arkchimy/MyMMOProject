#pragma once
#include <vector>
#include "Image.h"

namespace render
{
	enum class eAnimationType : __int8
	{
		Loop,
		EndStop,
	};
	class Animation
	{
	public:
		Animation(const eAnimationType type, const int& speed = 5);
		~Animation();
		// TODO : Animation 대입, 이동 구현하기
		Animation(const Animation& other) = delete;
		Animation(const Animation&& other) = delete;
		const Animation& operator = (const Animation& rhs) = delete;
		const Animation& operator = (const Animation&& rhs) = delete;
	public:
		void AddImage(const char* const filename);
		void SetAtlas(const char* filename, const int frameCount, const int cols, const int frameW, const int frameH, const int startRow);
		bool Run();
		void FastForward(const int ticks);
		void Initialize();

		ID3D11ShaderResourceView* GetCurrentSRV() const;
		ID3D11SamplerState* GetCurrentSampleState() const;
		void GetCurrentUVOffset(float& uvX, float& uvY);
		void GetCurrentUVScale(float& scaleX, float& scaleY);

		inline int GetSpriteWidth() const { return mFrameW; }
		inline int GetSpriteHeight() const { return mFrameH; }

	private:
		const int mSpeed;
		int mSpeedCnt;

		const char* mFilename;
		const Image* mAtlasImage;

		int mCurrentFrame;

		int mFrameCount;
		int mCols;
		int mFrameW;
		int mFrameH;
		int mStartRow;
		const eAnimationType mType;
		ID3D11ShaderResourceView* mSRV;
	};
}
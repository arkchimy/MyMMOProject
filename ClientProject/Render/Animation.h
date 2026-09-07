#pragma once
#include <cstdint>
#include <vector>
#include "Image.h"

namespace render
{
	enum class eAnimationType : int8_t
	{
		Loop,
		EndStop,
	};
	class Animation final
	{
	public:
		Animation(const eAnimationType type, const int32_t& speed = 5);
		~Animation();
		// TODO : Animation 대입, 이동 구현하기
		Animation(const Animation& other) = delete;
		Animation(Animation&& other) = delete;
		const Animation& operator = (const Animation& rhs) = delete;
		Animation& operator=(Animation&& rhs) = delete;
	public:
		void AddImage(const char* const filename);
		void SetAtlas(const char* filename, const int32_t frameCount, const int32_t cols, const int32_t frameW, const int32_t frameH, const int32_t startRow);
		bool Run();
		void FastForward(const int32_t ticks);
		void Initialize();

		ID3D11ShaderResourceView* GetCurrentSRV() const;
		ID3D11SamplerState* GetCurrentSampleState() const;
		void GetCurrentUVOffset(float& uvX, float& uvY);
		void GetCurrentUVScale(float& scaleX, float& scaleY);

		inline int32_t GetSpriteWidth() const { return mFrameW; }
		inline int32_t GetSpriteHeight() const { return mFrameH; }

	private:
		const int32_t mSpeed;
		int32_t mSpeedCnt;

		const char* mFilename;
		const Image* mAtlasImage;

		int32_t mCurrentFrame;

		int32_t mFrameCount;
		int32_t mCols;
		int32_t mFrameW;
		int32_t mFrameH;
		int32_t mStartRow;
		const eAnimationType mType;
		ID3D11ShaderResourceView* mSRV;
	};
}
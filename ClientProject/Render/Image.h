#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <unordered_map>
struct ID3D11Texture2D;
struct ID3D11ShaderResourceView;
struct ID3D11SamplerState;

namespace render
{
	class Image final
	{
		friend class Animation;
	public:
		Image();
		~Image();

		Image(const Image&) = delete;
		Image& operator=(const Image&) = delete;
		Image(Image&&) = delete;
		Image& operator=(Image&&) = delete;

		void ReadFromFile(const char* filename);
		inline const char* GetFilename() const { return mFilename; }
		ID3D11ShaderResourceView* GetSRV() const { return mSRV; }
		ID3D11SamplerState* GetSampleState() const { return mSampleState; }
		inline int32_t GetWidth() const { return mWidth; }
		inline int32_t GetHeight() const { return mHeight; }
	private:
		int32_t mWidth;
		int32_t mHeight;
		const char* mFilename;

		ID3D11Texture2D* mTexture;
		ID3D11ShaderResourceView* mSRV;
		ID3D11SamplerState* mSampleState;
	};
}
/*
	vcpkg install stb:x64-windows
	프로젝트 설정에서 _CRT_SECURE_NO_WARNINGS 추가 ('sprintf' in stb_image_write.h)
	#define STB_IMAGE_IMPLEMENTATION
	#include <stb_image.h>
	#define STB_IMAGE_WRITE_IMPLEMENTATION
	#include <stb_image_write.h>
*/
#pragma warning(push, 0)
#pragma warning(disable: 26819)
#pragma warning(disable: 6262)

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#pragma warning(pop)

#include "Image.h"
#include "Game.h"
#include "utility/Common.h"

#include <d3d11.h>
#include <d3dcompiler.h>


namespace render
{
	Image::Image()
		: mTexture(nullptr)
		, mSRV(nullptr)
		, mSampleState(nullptr)
		, mWidth(0)
		, mHeight(0)
		, mFilename(nullptr)
	{
	}

	Image::~Image()
	{
		if (mTexture != nullptr)
		{
			mTexture->Release();
		}
		if (mSRV != nullptr)
		{
			mSRV->Release();
		}
		if (mSampleState != nullptr)
		{
			mSampleState->Release();
		}
	}

	void Image::ReadFromFile(const char* filename)
	{
		RT_ASSERT(mFilename == nullptr);

		unsigned char* img = stbi_load(filename, &mWidth, &mHeight, nullptr, 4);
		if (mWidth == 0)
		{
			// LastError 2 : ERROR_FILE_NOT_FOUND
			volatile DWORD lasterror = GetLastError();
			__debugbreak();
		}
		mFilename = filename;
		// Texture2D생성
		D3D11_TEXTURE2D_DESC desc;
		{
			desc.Width = mWidth;                         // 이미지 가로 픽셀 수
			desc.Height = mHeight;                         // 이미지 세로 픽셀 수
			desc.MipLevels = 1;                          // 밉맵 없음 (스프라이트니까)
			desc.ArraySize = 1;                          // 텍스처 1장
			desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; // RGBA 각 1바이트
			desc.SampleDesc.Count = 1;                          // 멀티샘플링 없음
			desc.SampleDesc.Quality = 0;
			desc.Usage = D3D11_USAGE_IMMUTABLE;      // CPU가 다시 안 씀, GPU 읽기 전용
			desc.BindFlags = D3D11_BIND_SHADER_RESOURCE; // SRV로 쓸 거
			desc.CPUAccessFlags = 0;                          // IMMUTABLE이면 0이어야 함
			desc.MiscFlags = 0;
		}

		D3D11_SUBRESOURCE_DATA initData = {};
		{
			initData.pSysMem = img;      // stbi_load가 뱉은 포인터
			initData.SysMemPitch = mWidth * 4;      // 가로 픽셀 수 * 4바이트(RGBA)
			initData.SysMemSlicePitch = 0;           // 2D 텍스처는 0
		}

		Game::GetInstance().GetDevice().CreateTexture2D(&desc, &initData, &mTexture);
		RT_ASSERT(mTexture != nullptr);

		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		{
			srvDesc.Format = desc.Format;           // Texture2D랑 같은 포맷
			srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
			srvDesc.Texture2D.MostDetailedMip = 0;                     // 밉맵 0번 (유일한 밉맵)
			srvDesc.Texture2D.MipLevels = 1;                     // 밉맵 1개
		}
		Game::GetInstance().GetDevice().CreateShaderResourceView(mTexture, &srvDesc, &mSRV);
		RT_ASSERT(mSRV != nullptr);
		D3D11_SAMPLER_DESC samplerDesc = {};
		{
			samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT; // 픽셀아트니까 POINT (선명하게)
			samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
			samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
			samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
		}

		Game::GetInstance().GetDevice().CreateSamplerState(&samplerDesc, &mSampleState);
		RT_ASSERT(mSampleState != nullptr);
		stbi_image_free(img);
	}
};
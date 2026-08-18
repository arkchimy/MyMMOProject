#pragma once
// UIBase.h
#include <d3d11.h>
#include <d3dcompiler.h>
namespace render
{
	class Animation;
}
namespace ui
{
	class UIBase
	{
	public:
		UIBase();
		virtual ~UIBase() = default;

	public:
		virtual void Update() = 0;
		virtual void Render();

	protected:
		void createConstantBuffer(ID3D11Buffer** constantBuffer);
		void createVertexBuffer(ID3D11Buffer** vertexBuffer);
		void updateConstantBuffer();

	protected:
		float mX;
		float mY;
		float mScale;
		render::Animation* mSprite;
		ID3D11Buffer* mVertexBuffer;
		ID3D11Buffer* mConstantBuffer;
	};

}

#pragma once
namespace map
{
    class TileMap;
}
namespace actors
{
    class Actor;
};
namespace scene
{
    class SceneManager;
};

#ifndef _WINDEF_
    struct HWND__;       typedef HWND__* HWND;
    struct HINSTANCE__;  typedef HINSTANCE__* HINSTANCE;
#endif

class Game
{
  private:
    Game();

  public:
    static Game &GetInstance()
    {
        static Game instance;
        return instance;
    }
    ~Game();
    void Start();

    HWND &GetWindows() { return mHwnd; }
    struct ID3D11DeviceContext &GetDeviceContext() const { return *mDeviceContext; }
    struct ID3D11Device &GetDevice() const { return *mDevice; }
    struct ID3D11Buffer &GetIndexBuffer() const { return *mIndexBuffer; }
    __int32 GetIndexCnt() const { return mIndexCount; }

    void Update();
    void Render();
    void Clean();
  private:
    void initShaders();
    void initialize(HWND& window, int width, int height);
    void createIndexBuffer();
    void createBlendState();
  private:
    struct ID3D11Device *mDevice;
    struct ID3D11DeviceContext *mDeviceContext;
    struct ID3D11BlendState* mBlendState;

    HWND mHwnd;
    HINSTANCE mWcInstance;

    struct IDXGISwapChain *mSwapChain;
    struct D3D11_VIEWPORT* mViewport;
    struct ID3D11RenderTargetView *mRenderTargetView;
    struct ID3D11VertexShader *mVertexShader;
    struct ID3D11PixelShader *mPixelShader;
    struct ID3D11InputLayout *mLayout;

    struct ID3D11Buffer *mIndexBuffer = nullptr;

    __int32 mIndexCount;

private:
    scene::SceneManager* const mSceneManager;
};

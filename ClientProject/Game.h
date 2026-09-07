#pragma once
#include <cstdint>
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

class Game final
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

    Game(const Game&) = delete;
    Game& operator=(const Game&) = delete;
    Game(Game&&) = delete;
    Game& operator=(Game&&) = delete;

    void Start();

    HWND &GetWindows() { return mHwnd; }
    struct ID3D11DeviceContext &GetDeviceContext() const { return *mDeviceContext; }
    struct ID3D11Device &GetDevice() const { return *mDevice; }
    struct ID3D11Buffer &GetIndexBuffer() const { return *mIndexBuffer; }
    int32_t GetIndexCnt() const { return mIndexCount; }

    void Update();
    void Render();
    void Clean();
  private:
    void initShaders();
    void initialize(HWND& window, int32_t width, int32_t height);
    void createIndexBuffer();
    void createBlendState();
  private:
    struct ID3D11Device *mDevice = nullptr;
    struct ID3D11DeviceContext *mDeviceContext = nullptr;
    struct ID3D11BlendState* mBlendState = nullptr;

    HWND mHwnd;
    HINSTANCE mWcInstance;

    struct IDXGISwapChain *mSwapChain = nullptr;
    struct D3D11_VIEWPORT* mViewport = nullptr;
    struct ID3D11RenderTargetView *mRenderTargetView = nullptr;
    struct ID3D11VertexShader *mVertexShader = nullptr;
    struct ID3D11PixelShader *mPixelShader = nullptr;
    struct ID3D11InputLayout *mLayout = nullptr;

    struct ID3D11Buffer *mIndexBuffer = nullptr;

    int32_t mIndexCount = 0;

private:
    scene::SceneManager* const mSceneManager;
};

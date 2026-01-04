// GTA5-ASI-MenuBase
// © 2026 kiraa024
// MIT License

#include <Windows.h>
#include <d3d11.h>

#include "MinHook/MinHook.h"

#include "ImGui/imgui.h"
#include "ImGui/imgui_impl_win32.h"
#include "ImGui/imgui_impl_dx11.h"

// ScriptHookV
#include "ScriptHookV/script.h"
#include "ScriptHookV/natives.h"

#pragma comment(lib, "d3d11.lib")

#if _WIN64
#pragma comment(lib, "MinHook/libMinHook.x64.lib")
#else
#pragma comment(lib, "MinHook/libMinHook.x86.lib")
#endif

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

namespace Overlay
{
    HWND hwnd = nullptr;
    WNDPROC originalWndProc = nullptr;

    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* context = nullptr;
    ID3D11RenderTargetView* rtv = nullptr;

    using PresentFn = long(__stdcall*)(IDXGISwapChain*, UINT, UINT);
    PresentFn originalPresent = nullptr;

    bool initialized = false;
    bool menuVisible = false;

    // --------------------------------------------------
    // GTA-native input blocking (correct)
    // --------------------------------------------------
    void BlockGameInput()
    {
        // Gameplay
        invoke<Void>(0x5F4B6931816E599B, 0); // DISABLE_ALL_CONTROL_ACTIONS(0)
        // Camera
        invoke<Void>(0x5F4B6931816E599B, 1); // DISABLE_ALL_CONTROL_ACTIONS(1)
        // Vehicle
        invoke<Void>(0x5F4B6931816E599B, 2); // DISABLE_ALL_CONTROL_ACTIONS(2)

        // Allow ESC / pause menu
        invoke<Void>(0xFE99B66D079CF6BC, 0, 200, true); // ENABLE_CONTROL_ACTION(0, ControlFrontendPause, true)
    }

    // --------------------------------------------------
    // WndProc hook (ImGui only)
    // --------------------------------------------------
    LRESULT CALLBACK WindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        if (menuVisible && ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
            return true;

        return CallWindowProc(originalWndProc, hWnd, msg, wParam, lParam);
    }

    // --------------------------------------------------
    // Setup render target
    // --------------------------------------------------
    void SetupRenderTarget(IDXGISwapChain* swapChain)
    {
        ID3D11Texture2D* backBuffer = nullptr;
        swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backBuffer);
        device->CreateRenderTargetView(backBuffer, nullptr, &rtv);
        backBuffer->Release();
    }

    // --------------------------------------------------
    // Hooked Present
    // --------------------------------------------------
    long __stdcall HookedPresent(IDXGISwapChain* swapChain, UINT interval, UINT flags)
    {
        if (!initialized)
        {
            if (SUCCEEDED(swapChain->GetDevice(__uuidof(ID3D11Device), (void**)&device)))
            {
                device->GetImmediateContext(&context);

                DXGI_SWAP_CHAIN_DESC desc;
                swapChain->GetDesc(&desc);
                hwnd = desc.OutputWindow;

                SetupRenderTarget(swapChain);
                originalWndProc = (WNDPROC)SetWindowLongPtr(hwnd, GWLP_WNDPROC, (LONG_PTR)WindowProc);

                IMGUI_CHECKVERSION();
                ImGui::CreateContext();

                ImGuiIO& io = ImGui::GetIO();
                io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;

                ImGui_ImplWin32_Init(hwnd);
                ImGui_ImplDX11_Init(device, context);

                initialized = true;
            }
        }

        // Toggle menu
        if (GetAsyncKeyState(VK_F4) & 1)
            menuVisible = !menuVisible;

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        // --------------------------------------------------
        // Input handling
        // --------------------------------------------------
        if (menuVisible)
        {
            BlockGameInput();

            ImGuiIO& io = ImGui::GetIO();
            io.WantCaptureMouse = true;
            io.WantCaptureKeyboard = true;
            io.MouseDrawCursor = true;
        }

        // --------------------------------------------------
        // ImGui menu
        // --------------------------------------------------
        if (menuVisible)
        {
            ImGui::Begin("GTA5 ASI Menu Base", nullptr,
                ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);

            ImGui::Text("Press F4 to toggle menu");
            ImGui::Text("Input is blocked using GTA natives");

            ImGui::End();
        }

        ImGui::Render();
        context->OMSetRenderTargets(1, &rtv, nullptr);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        return originalPresent(swapChain, interval, flags);
    }

    // --------------------------------------------------
    // Get Present pointer
    // --------------------------------------------------
    PresentFn GetPresentFunction()
    {
        D3D_FEATURE_LEVEL level = D3D_FEATURE_LEVEL_11_0;
        DXGI_SWAP_CHAIN_DESC desc = {};
        desc.BufferCount = 1;
        desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        desc.OutputWindow = GetDesktopWindow();
        desc.SampleDesc.Count = 1;
        desc.Windowed = TRUE;

        ID3D11Device* tempDevice = nullptr;
        IDXGISwapChain* tempSwap = nullptr;

        if (D3D11CreateDeviceAndSwapChain(
            nullptr,
            D3D_DRIVER_TYPE_HARDWARE,
            nullptr,
            0,
            &level,
            1,
            D3D11_SDK_VERSION,
            &desc,
            &tempSwap,
            &tempDevice,
            nullptr,
            nullptr) == S_OK)
        {
            void** vtable = *(void***)tempSwap;
            PresentFn fn = (PresentFn)vtable[8];

            tempSwap->Release();
            tempDevice->Release();
            return fn;
        }

        return nullptr;
    }

    // --------------------------------------------------
    // Init thread
    // --------------------------------------------------
    DWORD WINAPI InitThread(LPVOID)
    {
        MH_Initialize();

        PresentFn presentAddr = GetPresentFunction();
        if (presentAddr)
        {
            MH_CreateHook(presentAddr, &HookedPresent, (void**)&originalPresent);
            MH_EnableHook(presentAddr);
        }

        while (!(GetAsyncKeyState(VK_END) & 0x8000))
            Sleep(50);

        MH_DisableHook(presentAddr);
        MH_Uninitialize();

        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();

        if (rtv) rtv->Release();
        if (context) context->Release();
        if (device) device->Release();

        SetWindowLongPtr(hwnd, GWLP_WNDPROC, (LONG_PTR)originalWndProc);
        FreeLibraryAndExitThread(GetModuleHandle(nullptr), 0);
    }
}

BOOL WINAPI DllMain(HINSTANCE hModule, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(hModule);
        CreateThread(nullptr, 0, Overlay::InitThread, nullptr, 0, nullptr);
    }
    return TRUE;
}

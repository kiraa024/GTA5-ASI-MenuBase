#include <Windows.h>
#include <d3d11.h>
#include "MinHook/MinHook.h"
#include "ImGui/imgui.h"
#include "ImGui/imgui_impl_win32.h"
#include "ImGui/imgui_impl_dx11.h"

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

    typedef long(__stdcall* PresentFn)(IDXGISwapChain*, UINT, UINT);
    PresentFn originalPresent = nullptr;

    bool initialized = false;
    bool menuVisible = false;

    // -----------------------------
    // Low-level input hooks
    // -----------------------------
    HHOOK keyboardHook = nullptr;
    HHOOK mouseHook = nullptr;

    LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam)
    {
        if (menuVisible && nCode >= 0)
            return 1; // eat input
        return CallNextHookEx(nullptr, nCode, wParam, lParam);
    }

    LRESULT CALLBACK LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam)
    {
        if (menuVisible && nCode >= 0)
            return 1; // eat input
        return CallNextHookEx(nullptr, nCode, wParam, lParam);
    }

    void EnableInputBlock()
    {
        keyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, GetModuleHandle(nullptr), 0);
        mouseHook = SetWindowsHookEx(WH_MOUSE_LL, LowLevelMouseProc, GetModuleHandle(nullptr), 0);
    }

    void DisableInputBlock()
    {
        if (keyboardHook) UnhookWindowsHookEx(keyboardHook);
        if (mouseHook) UnhookWindowsHookEx(mouseHook);
    }

    // -----------------------------
    // WndProc hook for ImGui
    // -----------------------------
    LRESULT CALLBACK WindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        if (menuVisible && ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
            return true;

        if (menuVisible)
        {
            switch (msg)
            {
            case WM_KEYDOWN: case WM_KEYUP:
            case WM_SYSKEYDOWN: case WM_SYSKEYUP:
            case WM_MOUSEMOVE:
            case WM_LBUTTONDOWN: case WM_LBUTTONUP:
            case WM_RBUTTONDOWN: case WM_RBUTTONUP:
            case WM_MBUTTONDOWN: case WM_MBUTTONUP:
            case WM_MOUSEWHEEL:
            case WM_XBUTTONDOWN: case WM_XBUTTONUP:
                return 0; // eat input
            default: break;
            }
        }

        return CallWindowProc(originalWndProc, hWnd, msg, wParam, lParam);
    }

    // -----------------------------
    // Setup render target
    // -----------------------------
    void SetupRenderTarget(IDXGISwapChain* swapChain)
    {
        ID3D11Texture2D* backBuffer = nullptr;
        swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&backBuffer));
        device->CreateRenderTargetView(backBuffer, nullptr, &rtv);
        backBuffer->Release();
    }

    // -----------------------------
    // Hooked Present
    // -----------------------------
    long __stdcall HookedPresent(IDXGISwapChain* swapChain, UINT interval, UINT flags)
    {
        if (!initialized)
        {
            if (SUCCEEDED(swapChain->GetDevice(__uuidof(ID3D11Device), reinterpret_cast<void**>(&device))))
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
        if (GetAsyncKeyState(VK_F4) & 0x1)
        {
            menuVisible = !menuVisible;
            if (menuVisible) EnableInputBlock();
            else DisableInputBlock();
        }

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        // Capture mouse & keyboard
        ImGuiIO& io = ImGui::GetIO();
        io.MouseDrawCursor = menuVisible;
        io.WantCaptureMouse = menuVisible;
        io.WantCaptureKeyboard = menuVisible;
        ::ShowCursor(menuVisible ? TRUE : FALSE);

        // -----------------------------
        // ImGui Menu
        // -----------------------------
        if (menuVisible)
        {
            ImGui::Begin("GTA5 ASI Menu Base", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
            ImGui::Text("Press F4 to toggle menu");
            ImGui::Text("Game input is fully blocked while menu is open");
            ImGui::End();
        }

        ImGui::Render();
        context->OMSetRenderTargets(1, &rtv, nullptr);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        return originalPresent(swapChain, interval, flags);
    }

    // -----------------------------
    // Get Present pointer
    // -----------------------------
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
        desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

        ID3D11Device* tempDevice = nullptr;
        IDXGISwapChain* tempSwap = nullptr;

        if (D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
            &level, 1, D3D11_SDK_VERSION, &desc, &tempSwap, &tempDevice, nullptr, nullptr) == S_OK)
        {
            void** vtable = *reinterpret_cast<void***>(tempSwap);
            PresentFn addr = reinterpret_cast<PresentFn>(vtable[8]);
            tempSwap->Release();
            tempDevice->Release();
            return addr;
        }
        return nullptr;
    }

    // -----------------------------
    // Initialization thread
    // -----------------------------
    DWORD WINAPI InitThread(LPVOID)
    {
        MH_Initialize();
        PresentFn presentAddr = GetPresentFunction();
        if (presentAddr)
        {
            MH_CreateHook(presentAddr, &HookedPresent, reinterpret_cast<void**>(&originalPresent));
            MH_EnableHook(presentAddr);
        }

        while (!(GetAsyncKeyState(VK_END) & 0x8000))
            Sleep(50);

        MH_DisableHook(presentAddr);
        MH_Uninitialize();

        DisableInputBlock();

        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();

        if (rtv) { rtv->Release(); rtv = nullptr; }
        if (context) { context->Release(); context = nullptr; }
        if (device) { device->Release(); device = nullptr; }
        SetWindowLongPtr(hwnd, GWLP_WNDPROC, (LONG_PTR)originalWndProc);

        FreeLibraryAndExitThread((HINSTANCE)GetModuleHandle(nullptr), 0);
        return 0;
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

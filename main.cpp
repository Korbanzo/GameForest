// Dear ImGui: standalone example application for Windows API + DirectX 11

// Learn about Dear ImGui:
// - FAQ                  https://dearimgui.com/faq
// - Getting Started      https://dearimgui.com/getting-started
// - Documentation        https://dearimgui.com/docs (same as your local docs/ folder).
// - Introduction, links and more at the top of imgui.cpp

/*
ADD
- Check if the entry made is already in the vector
- Auto Validate and take games out of the json if the path is no longer valid
- Sort by alphabetical or size or something
*/

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include <d3d11.h>
#include <tchar.h>
#include <iostream>
#include <wrl/client.h>
#include <filesystem>
#include <fstream>
#include <vector>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

// Data
static ID3D11Device* g_pd3dDevice = nullptr;
static ID3D11DeviceContext* g_pd3dDeviceContext = nullptr;
static IDXGISwapChain* g_pSwapChain = nullptr;
static bool                     g_SwapChainOccluded = false;
static UINT                     g_ResizeWidth = 0, g_ResizeHeight = 0;
static ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;
static const size_t maxPathLength = 512;

// Forward declarations of helper functions
bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

struct Game {
    std::string name;
    std::string path;
};

std::vector<Game> games;

void myStyle() {
    ImGuiStyle& style = ImGui::GetStyle();

    // --- Monochrome moss palette ---
    ImVec4 mossVoid = { 0.04f, 0.06f, 0.05f, 1.00f }; // darkest
    ImVec4 mossPanel = { 0.07f, 0.10f, 0.08f, 1.00f };
    ImVec4 mossFrame = { 0.10f, 0.14f, 0.11f, 1.00f };
    ImVec4 mossHover = { 0.13f, 0.18f, 0.14f, 1.00f };
    ImVec4 mossActive = { 0.16f, 0.22f, 0.17f, 1.00f };
    ImVec4 mossText = { 0.86f, 0.90f, 0.88f, 1.00f };
    ImVec4 mossTextMute = { 0.52f, 0.58f, 0.55f, 1.00f };
    ImVec4 mossBorder = { 0.12f, 0.16f, 0.13f, 1.00f };

    // --- Backgrounds ---
    style.Colors[ImGuiCol_WindowBg] = mossVoid;
    style.Colors[ImGuiCol_ChildBg] = mossPanel;
    style.Colors[ImGuiCol_PopupBg] = mossPanel;

    // --- Frames ---
    style.Colors[ImGuiCol_FrameBg] = mossFrame;
    style.Colors[ImGuiCol_FrameBgHovered] = mossHover;
    style.Colors[ImGuiCol_FrameBgActive] = mossActive;

    // --- Buttons ---
    style.Colors[ImGuiCol_Button] = mossFrame;
    style.Colors[ImGuiCol_ButtonHovered] = mossHover;
    style.Colors[ImGuiCol_ButtonActive] = mossActive;

    // --- Headers ---
    style.Colors[ImGuiCol_Header] = mossFrame;
    style.Colors[ImGuiCol_HeaderHovered] = mossHover;
    style.Colors[ImGuiCol_HeaderActive] = mossActive;

    // --- Title bar ---
    style.Colors[ImGuiCol_TitleBg] = mossPanel;
    style.Colors[ImGuiCol_TitleBgActive] = mossFrame;
    style.Colors[ImGuiCol_TitleBgCollapsed] = mossPanel;

    // --- Text ---
    style.Colors[ImGuiCol_Text] = mossText;
    style.Colors[ImGuiCol_TextDisabled] = mossTextMute;

    // --- Borders ---
    style.Colors[ImGuiCol_Border] = mossBorder;
    style.Colors[ImGuiCol_BorderShadow] = { 0, 0, 0, 0 };

    // --- Scrollbars ---
    style.Colors[ImGuiCol_ScrollbarBg] = mossVoid;
    style.Colors[ImGuiCol_ScrollbarGrab] = mossFrame;
    style.Colors[ImGuiCol_ScrollbarGrabHovered] = mossHover;
    style.Colors[ImGuiCol_ScrollbarGrabActive] = mossActive;

    // --- Tabs ---
    style.Colors[ImGuiCol_Tab] = mossPanel;
    style.Colors[ImGuiCol_TabHovered] = mossHover;
    style.Colors[ImGuiCol_TabActive] = mossFrame;
    style.Colors[ImGuiCol_TabUnfocused] = mossPanel;
    style.Colors[ImGuiCol_TabUnfocusedActive] = mossFrame;

    // --- Sliders ---
    style.Colors[ImGuiCol_SliderGrab] = mossHover;
    style.Colors[ImGuiCol_SliderGrabActive] = mossActive;

    // --- Shape & spacing ---
    style.WindowRounding = 6.0f;
    style.FrameRounding = 5.0f;
    style.PopupRounding = 5.0f;
    style.ScrollbarRounding = 6.0f;
    style.GrabRounding = 5.0f;
    style.TabRounding = 5.0f;

    style.FramePadding = ImVec2(8, 5);
    style.ItemSpacing = ImVec2(8, 6);
}

void formatPathStr(std::string& path) {
    std::replace(path.begin(), path.end(), '\\', '/');
    path.erase(std::remove(path.begin(), path.end(), '"'), path.end());
}

std::string getGameName(std::string& path) {
    size_t currIdx = path.length() - 1;
    size_t currItr = 0;
    int fwdSlashCount = 0;
    int nameLength = -1;

    while (fwdSlashCount < 2 && currItr <= maxPathLength) {
        if (fwdSlashCount == 1) {
            nameLength++;
        }

        if (path[currIdx] == '/') {
            if (fwdSlashCount == 1) {
                return path.substr(currIdx + 1, nameLength);
            }

            fwdSlashCount++;
        }

        currIdx--;
        currItr++;
    }

    return "No name :(";
}

bool isUniqueGame(const std::vector<Game>& games, std::string& path) {
    for (Game game : games) {
        if (game.name == getGameName(path)) {
            return false;
        }
    }

    return true;
}

void openGame(std::string& path) {
    ShellExecuteA(nullptr, "open", path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

void to_json(json& j, const Game& game) {
    j = json{ {"name", game.name}, {"path", game.path} };
}

void from_json(const json& j, Game& game) {
    j.at("name").get_to(game.name);
    j.at("path").get_to(game.path);
}

const std::string GAMES_FILE = "games.json";

void saveGames(const std::vector<Game>& games) {
    json j = games;
    std::ofstream out(GAMES_FILE);
    if (!out.is_open()) return;
    out << j.dump(4); // 4 space tabs (goated)
}

void loadGames(std::vector<Game>& games) {
    std::ifstream in(GAMES_FILE);
    if (!in.is_open()) return;

    try {
        json j;
        in >> j;
        games = j.get<std::vector<Game>>();
    }
    catch (...) {
        games.clear();
    }
}

// Main code
int main(int, char**)
{
    // Make process DPI aware and obtain main monitor scale
    ImGui_ImplWin32_EnableDpiAwareness();
    float main_scale = ImGui_ImplWin32_GetDpiScaleForMonitor(::MonitorFromPoint(POINT{ 0, 0 }, MONITOR_DEFAULTTOPRIMARY));

    // Create application window
    WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, L"ImGui Example", nullptr };
    ::RegisterClassExW(&wc);
    HWND hwnd = ::CreateWindowW(wc.lpszClassName, L"Game Forest", WS_OVERLAPPEDWINDOW, 100, 100, (int)(1280 * main_scale), (int)(800 * main_scale), nullptr, nullptr, wc.hInstance, nullptr);

    if (!hwnd) {
        MessageBoxA(nullptr, "Window creation failed!", "Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    // Initialize Direct3D
    if (!CreateDeviceD3D(hwnd))
    {
        MessageBoxA(nullptr, "Direct3D device creation failed!", "Error", MB_OK | MB_ICONERROR);
        CleanupDeviceD3D();
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    // Show the window
    ::ShowWindow(hwnd, SW_SHOWDEFAULT);
    ::UpdateWindow(hwnd);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    myStyle();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsLight();

    // Setup scaling
    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(main_scale);        // Bake a fixed style scale. (until we have a solution for dynamic style scaling, changing this requires resetting Style + calling this again)
    style.FontScaleDpi = main_scale;        // Set initial font scale. (using io.ConfigDpiScaleFonts=true makes this unnecessary. We leave both here for documentation purpose)

    // Setup Platform/Renderer backends
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    // Load Fonts
    // - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
    // - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
    // - If the file cannot be loaded, the function will return a nullptr. Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
    // - Use '#define IMGUI_ENABLE_FREETYPE' in your imconfig file to use Freetype for higher quality font rendering.
    // - Read 'docs/FONTS.md' for more instructions and details. If you like the default font but want it to scale better, consider using the 'ProggyVector' from the same author!
    // - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
    //style.FontSizeBase = 20.0f;
    //io.Fonts->AddFontDefault();
    //io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\segoeui.ttf");
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf");
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf");
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf");
    //ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf");
    //IM_ASSERT(font != nullptr);

    // Our state ***************************************************************************************
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    ImVec2 winSize = { 1000, 600 };
    ImGui::SetNextWindowSize(winSize);

    bool done = false;
    bool open = true;

    char pathToExe[maxPathLength] = {};
    bool showPathToExe = false;

    loadGames(games);

    // Main loop
    while (!done)
    {
        // Poll and handle messages (inputs, window resize, etc.)
        // See the WndProc() function below for our to dispatch events to the Win32 backend.
        MSG msg;
        while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
        {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                done = true;
        }
        if (done)
            break;

        // Handle window being minimized or screen locked
        if (g_SwapChainOccluded && g_pSwapChain->Present(0, DXGI_PRESENT_TEST) == DXGI_STATUS_OCCLUDED)
        {
            ::Sleep(10);
            continue;
        }
        g_SwapChainOccluded = false;

        // Handle window resize (we don't resize directly in the WM_SIZE handler)
        if (g_ResizeWidth != 0 && g_ResizeHeight != 0)
        {
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, g_ResizeWidth, g_ResizeHeight, DXGI_FORMAT_UNKNOWN, 0);
            g_ResizeWidth = g_ResizeHeight = 0;
            CreateRenderTarget();
        }

        // Start the Dear ImGui frame
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame(); // Start ==================================================================================================================================


        if (ImGui::Begin("Game Forest", &open, ImGuiWindowFlags_NoCollapse)) {

            if (ImGui::Button("Add Game", ImVec2{ 0, 0 })) {
                showPathToExe = !showPathToExe;

                if (!showPathToExe && strlen(pathToExe) > 0) {
                    std::string path = pathToExe;
                    formatPathStr(path);

                    std::filesystem::path fileSystemPath(path);

                    if (path.ends_with(".exe") && std::filesystem::exists(fileSystemPath) && isUniqueGame(games, path)) {
                        games.push_back({ getGameName(path), path });
                        saveGames(games);
                    }

                    memset(pathToExe, 0, sizeof(pathToExe));
                }
            }

            std::string gamesInstalledText = std::format("{} Games Installed", games.size());
            ImGui::Text(gamesInstalledText.c_str(), ImVec2{ 0 , 30 });

            if (showPathToExe) {
                ImGui::InputText("Path to .exe", pathToExe, IM_ARRAYSIZE(pathToExe));
            }

            for (auto& game : games) {
                if (ImGui::Button(game.name.c_str())) {
                    openGame(game.path);
                }
            }
        }

        ImGui::End(); // End =========================================================================================================================================

        myStyle();
        // Rendering
        ImGui::Render();
        const float clear_color_with_alpha[4] = { clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w };
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color_with_alpha);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        // Present
        HRESULT hr = g_pSwapChain->Present(1, 0);   // Present with vsync
        //HRESULT hr = g_pSwapChain->Present(0, 0); // Present without vsync
        g_SwapChainOccluded = (hr == DXGI_STATUS_OCCLUDED);
    }

    // Cleanup
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    ::DestroyWindow(hwnd);
    ::UnregisterClassW(wc.lpszClassName, wc.hInstance);

    return 0;
}

// Helper functions

bool CreateDeviceD3D(HWND hWnd)
{
    // Setup swap chain
    // This is a basic setup. Optimally could use e.g. DXGI_SWAP_EFFECT_FLIP_DISCARD and handle fullscreen mode differently. See #8979 for suggestions.
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    //createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
    HRESULT res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res == DXGI_ERROR_UNSUPPORTED) // Try high-performance WARP software driver if hardware is not available.
        res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res != S_OK)
        return false;

    CreateRenderTarget();
    return true;
}

void CleanupDeviceD3D()
{
    CleanupRenderTarget();
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = nullptr; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
}

void CreateRenderTarget()
{
    ID3D11Texture2D* pBackBuffer;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
    pBackBuffer->Release();
}

void CleanupRenderTarget()
{
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
}

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Win32 message handler
// You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
// - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
// - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
// Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
    case WM_SIZE:
        if (wParam == SIZE_MINIMIZED)
            return 0;
        g_ResizeWidth = (UINT)LOWORD(lParam); // Queue resize
        g_ResizeHeight = (UINT)HIWORD(lParam);
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
            return 0;
        break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }
    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd) {
    return main(0, nullptr);
}
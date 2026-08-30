#include <Windows.h>
#include <psapi.h>
#include <d3d11.h>

#include "Console.h"
#include "WinVersion.h"
#include "aob.h"
#include "backbuffer.h"
#include "minhook\include\MinHook.h"
#include "imgui\imgui.h"
#include "imgui\imgui_impl_dx11.h"

#pragma comment(lib, "dxguid.lib")

// 系统版本
extern WinSubVersion g_WinVersion;

// ImGui 相关
static ID3D11Device* g_pd3dDevice = nullptr;
static ID3D11DeviceContext* g_pd3dContext = nullptr;
static ID3D11RenderTargetView* g_prtView = nullptr;

// COverlayContext::Present 函数原型
typedef __int64 (COverlayContext_Present_old)(void* _this, void* a2, unsigned int a3, __int64 a4, int a5, bool a6);
static COverlayContext_Present_old* COverlayContext_Present_old_orig = nullptr;		// Windows 11 24H2 以前

typedef __int64 (COverlayContext_Present)(void* _this, void* a2, unsigned int a3, __int64 a4, int a5, void* a6, bool a7);
static COverlayContext_Present* COverlayContext_Present_orig = nullptr;

// ImGui渲染
static void StartImGuiRender(ID3D11Texture2D* D3D11Texture2D)
{
	// 初始化ImGui
	static bool ImGuiInitialize = false;
	if (!ImGuiInitialize)
	{
		ImGuiInitialize = true;

		D3D11Texture2D->GetDevice(&g_pd3dDevice);

		g_pd3dDevice->GetImmediateContext(&g_pd3dContext);

		//g_pd3dDevice->CreateRenderTargetView(D3D11Texture2D, nullptr, &g_prtView);

		ImGui::CreateContext();
		ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dContext);

		log("[+] ImGui initialized successfully\n");
	}

	// 获取后备缓冲区尺寸
	D3D11_TEXTURE2D_DESC bbDesc;
	D3D11Texture2D->GetDesc(&bbDesc);

	// 设置 ImGui 显示尺寸
	ImGui::GetIO().DisplaySize = ImVec2((float)bbDesc.Width, (float)bbDesc.Height);

	// 创建 RTV
	if (g_prtView) { g_prtView->Release(); g_prtView = nullptr; }
	g_pd3dDevice->CreateRenderTargetView(D3D11Texture2D, nullptr, &g_prtView);

	D3D11Texture2D->Release();

	// 保存原始渲染目标
	ID3D11RenderTargetView* prevRTV = nullptr;
	ID3D11DepthStencilView* prevDSV = nullptr;
	g_pd3dContext->OMGetRenderTargets(1, &prevRTV, &prevDSV);

	// 设置我们的渲染目标
	g_pd3dContext->OMSetRenderTargets(1, &g_prtView, nullptr);

	// 开始 ImGui 帧
	ImGui_ImplDX11_NewFrame();
	ImGui::NewFrame();

	// ########################################################
	// ############## 这里开始我们的自定义绘制 ################
	// ########################################################

	// 绘制矩形
	ImDrawList* draw_list = ImGui::GetForegroundDrawList();
	draw_list->AddRectFilled(ImVec2(100, 100), ImVec2(300, 200), IM_COL32(255, 0, 0, 255));

	// ########################################################
	// ######################## 结束 ##########################
	// ########################################################

	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

// Hook回调
static __int64 COverlayContext_Present_old_hook(void* _this, void* IOverlaySwapChain, unsigned int a3, __int64 a4, int a5, bool a6)
{
	// 从 overlaySwapChain 获取后备缓冲区
	ID3D11Texture2D* buf = GetBackBuffer(IOverlaySwapChain);

	// 开始自定义ImGui绘制
	StartImGuiRender(buf);

	// 调用原函数
	return COverlayContext_Present_old_orig(_this, IOverlaySwapChain, a3, a4, a5, a6);
}

static __int64 COverlayContext_Present_hook(void* _this, void* IOverlaySwapChain, unsigned int a3, __int64 a4, int a5, void* a6, bool a7)
{
	// 从 overlaySwapChain 获取后备缓冲区
	ID3D11Texture2D* buf = GetBackBuffer(IOverlaySwapChain);

	// 开始自定义ImGui绘制
	StartImGuiRender(buf);

	// 调用原函数
	return COverlayContext_Present_orig(_this, IOverlaySwapChain, a3, a4, a5, a6, a7);
}

static BOOL __stdcall DllMain(HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
	// Dll被加载
	if (ul_reason_for_call == DLL_PROCESS_ATTACH)
	{
		LoadConsole();
		log("[+] DLL attached to process %d\n", GetCurrentProcessId());

		// 获取系统版本
		GetWinVersion();
		if (g_WinVersion == UNKNOWN)
		{
			log("[-] failed to get windows version\n");
			return TRUE;
		}
		
		// 获取 dwmcore.dll 模块句柄
		HMODULE dwmcorebase = GetModuleHandleW(L"dwmcore.dll");
		if (!dwmcorebase)
		{
			log("[-] dwmcore.dll not found\n");
			return TRUE;
		}

		// 获取 dwmcore.dll 模块信息
		MODULEINFO moduleInfo;
		GetModuleInformation(GetCurrentProcess(), dwmcorebase, &moduleInfo, sizeof(moduleInfo));
		log("[+] dwmcore.dll moduleInfo: base=0x%llX, size=0x%X\n", moduleInfo.lpBaseOfDll, moduleInfo.SizeOfImage);

		// 搜索 COverlayContext::Present 函数位置
		void* present_addr = Get_COverlayContext_Present_Adr(moduleInfo);;
		if (!present_addr)
		{
			log("[-] COverlayContext::Present not found\n");
			return TRUE;
		}
		log("[+] Found COverlayContext::Present at 0x%llX\n", present_addr);

		// 初始化 MinHook 并挂钩函数 COverlayContext::Present
		if (MH_Initialize() != MH_OK)
		{
			log("[-] MH_Initialize failed");
			return TRUE;
		}

		if (g_WinVersion < WIN11_24H2)
		{
			if (MH_CreateHook(present_addr, (void*)COverlayContext_Present_old_hook, (void**)&COverlayContext_Present_old_orig) != MH_OK)
			{
				log("[+] MH_CreateHook failed\n");
				return TRUE;
			}
		} else {
			if (MH_CreateHook(present_addr, (void*)COverlayContext_Present_hook, (void**)&COverlayContext_Present_orig) != MH_OK)
			{
				log("[+] MH_CreateHook failed\n");
				return TRUE;
			}
		}

		if (MH_EnableHook(present_addr) != MH_OK)
		{
			log("[+] MH_EnableHook failed\n");
			return TRUE;
		}

		log("[+] Hook installed successfully\n");
	}

	// Dll被卸载
	if (ul_reason_for_call == DLL_PROCESS_DETACH)
	{
		UnLoadConsole();
	}

	return TRUE;
}
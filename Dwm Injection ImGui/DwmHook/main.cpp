#include <Windows.h>
#include <psapi.h>
#include <d3d11.h>

#include "Console.h"
#include "minhook/include/MinHook.h"
#include "imgui\imgui.h"
#include "imgui\imgui_impl_dx11.h"

static bool g_imguiinited = false;
static ID3D11Device* g_pd3dDevice = nullptr;
static IDXGISwapChain* g_pSwapChain = nullptr;
static ID3D11DeviceContext* g_pd3dContext = nullptr;
static ID3D11RenderTargetView* view = nullptr;

// 原函数
typedef __int64 (CDXGISwapChain_PresentDWM)(void* _this, int a2, unsigned int a3, int a4, PVOID a5, unsigned int a6, PVOID a7, PVOID a8, unsigned int a9);
static CDXGISwapChain_PresentDWM* CDXGISwapChain_PresentDWM_orig = nullptr;

// 特征码搜索
bool aob_match_inverse(const void* buf1, const void* mask, const int buf_len)
{
	for (int i = 0; i < buf_len; ++i)
	{
		if (((unsigned char*)buf1)[i] != ((unsigned char*)mask)[i] && ((unsigned char*)mask)[i] != '?')
			return true;
	}
	return false;
}

// Hook回调
static __int64 CDXGISwapChain_PresentDWM_hook(IDXGISwapChain* _this, int a2, unsigned int a3, int a4, PVOID a5, unsigned int a6, PVOID a7, PVOID a8, unsigned int a9)
{
	if (!g_imguiinited)
	{
		// 仅初始化一次
		g_imguiinited = true;
		
		// ImGui初始化
		_this->GetDevice(__uuidof(ID3D11Device), (void**)&g_pd3dDevice);
		g_pd3dDevice->GetImmediateContext(&g_pd3dContext);

		ID3D11Texture2D* buf = nullptr;
		_this->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&buf);
		if (!buf)
		{
			g_imguiinited = false;
			return CDXGISwapChain_PresentDWM_orig(_this, a2, a3, a4, a5, a6, a7, a8, a9);
		}

		g_pd3dDevice->CreateRenderTargetView(buf, nullptr, &view);
		buf->Release();

		ImGui::CreateContext();
		ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dContext);

		log("[+] ImGui initialized successfully\n");
	}

	// 获取当前后备缓冲区
	ID3D11Texture2D* backBuffer = nullptr;
	if (FAILED(_this->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backBuffer)))
		return CDXGISwapChain_PresentDWM_orig(_this, a2, a3, a4, a5, a6, a7, a8, a9);

	// 获取后备缓冲区尺寸
	D3D11_TEXTURE2D_DESC bbDesc;
	backBuffer->GetDesc(&bbDesc);

	// 设置 ImGui 显示尺寸
	ImGui::GetIO().DisplaySize = ImVec2((float)bbDesc.Width, (float)bbDesc.Height);

	// 释放旧 RTV 并创建新 RTV
	if (view) { view->Release(); view = nullptr; }
	if (FAILED(g_pd3dDevice->CreateRenderTargetView(backBuffer, nullptr, &view)))
	{
		backBuffer->Release();
		return CDXGISwapChain_PresentDWM_orig(_this, a2, a3, a4, a5, a6, a7, a8, a9);
	}
	backBuffer->Release();

	// 保存原始渲染目标
	ID3D11RenderTargetView* prevRTV = nullptr;
	ID3D11DepthStencilView* prevDSV = nullptr;
	g_pd3dContext->OMGetRenderTargets(1, &prevRTV, &prevDSV);

	// 设置我们的渲染目标
	g_pd3dContext->OMSetRenderTargets(1, &view, nullptr);

	// 开始 ImGui 帧
	ImGui_ImplDX11_NewFrame();
	ImGui::NewFrame();

	// 绘制矩形
	ImDrawList* draw_list = ImGui::GetForegroundDrawList();
	draw_list->AddRectFilled(ImVec2(100, 100), ImVec2(300, 200), IM_COL32(255, 0, 0, 255));

	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

	// 调用原函数
	return CDXGISwapChain_PresentDWM_orig(_this, a2, a3, a4, a5, a6, a7, a8, a9);
}

static BOOL __stdcall DllMain(HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
	// Dll被加载
	if (ul_reason_for_call == DLL_PROCESS_ATTACH)
	{
		LoadConsole();
		log("[+] DLL attached to process %d\n", GetCurrentProcessId());
		
		// 获取 dxgi.dll 模块句柄
		HMODULE dxgibase = GetModuleHandleW(L"dxgi.dll");
		if (!dxgibase)
		{
			log("[-] dxgi.dll not found\n");
			return TRUE;
		}

		// 获取 dxgi.dll 模块信息
		MODULEINFO moduleInfo;
		GetModuleInformation(GetCurrentProcess(), dxgibase, &moduleInfo, sizeof(moduleInfo));
		log("[+] dxgi.dll moduleInfo: base=0x%llX, size=0x%X\n", moduleInfo.lpBaseOfDll, moduleInfo.SizeOfImage);

		// CDXGISwapChain::PresentDWM 函数特征码
		const unsigned char PresentDWM_Bytes[68] = {
			0x48, 0x89, 0x5C, 0x24, 0x10, 0x55, 0x56, 0x57,
			0x41, 0x54, 0x41, 0x55, 0x41, 0x56, 0x41, 0x57,
			0x48,
			'?', '?', '?', '?', '?', '?', '?', '?',
			'?', '?', '?', '?', '?', '?', '?', '?',
			'?', '?', '?', '?', '?', '?', '?', '?',
			'?', '?', '?', '?', '?', '?', '?', '?',
			'?', '?', '?', '?', '?', '?', '?', '?',
			'?', '?', '?', '?',
			0x8B,
			'?', '?', '?', '?', '?',
			0x00
		};

		// 搜索特征码
		void* present_addr = nullptr;
		for (size_t i = 0; i < moduleInfo.SizeOfImage - sizeof(PresentDWM_Bytes); i++)
		{
			unsigned char* addr = (unsigned char*)dxgibase + i;
			if (!aob_match_inverse(addr, PresentDWM_Bytes, sizeof(PresentDWM_Bytes)))
			{
				present_addr = addr;
				log("[+] Found CDXGISwapChain::PresentDWM at 0x%llX\n", present_addr);
				break;
			}
		}

		if (!present_addr)
		{
			log("[-] CDXGISwapChain::PresentDWM signature not found\n");
			return TRUE;
		}

		// 初始化 MinHook 并挂钩函数 CDXGISwapChain::PresentDWM
		if (MH_Initialize() != MH_OK)
		{
			log("[-] MH_Initialize failed");
			return TRUE;
		}
		if (MH_CreateHook(present_addr, (void*)CDXGISwapChain_PresentDWM_hook, (void**)&CDXGISwapChain_PresentDWM_orig) != MH_OK)
		{
			log("[+] MH_CreateHook failed\n");
			return TRUE;
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
#pragma once

// 已 AddRef，调用者需 Release
ID3D11Texture2D* GetBackBuffer(void* overlaySwapChain)
{
    if (!overlaySwapChain) return nullptr;

    switch (g_WinVersion)
    {
    case WIN10:
    case WIN11_21H2:
    case WIN11_22H2_23H2:
    case WIN11_24H2:
    {
        IDXGISwapChain* swapChain = nullptr;
        __try {
            if (g_WinVersion == WIN10)
                swapChain = *(IDXGISwapChain**)((unsigned char*)overlaySwapChain - 0x118);
            else if (g_WinVersion == WIN11_21H2)
                swapChain = *(IDXGISwapChain**)((unsigned char*)overlaySwapChain - 0x148);
            else if (g_WinVersion == WIN11_22H2_23H2) 
            {
                int sub = *(int*)((unsigned char*)overlaySwapChain - 4);
                void* real = (unsigned char*)overlaySwapChain - sub - 0x1B0;
                swapChain = *(IDXGISwapChain**)((unsigned char*)real + 0xE0);
            }
            else if (g_WinVersion == WIN11_24H2)
                swapChain = *(IDXGISwapChain**)((unsigned char*)overlaySwapChain + 0x108);
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            return nullptr;
        }

        if (!swapChain) return nullptr;

        ID3D11Texture2D* backBuffer = nullptr;
        HRESULT hr = swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backBuffer);

        if (FAILED(hr) || !backBuffer) return nullptr;
        return backBuffer;
    }

    case WIN11_25H2:
    {
        __try {
            // 从 overlaySwapChain 获取虚函数表，调用索引 24 的函数 CLegacySwapChain::GetPhysicalBackBuffer
            void** vt = *(void***)overlaySwapChain;
            typedef void* (__fastcall* VirtFunc)(void*);
            VirtFunc func1 = (VirtFunc)vt[24];      // 索引 24
            void* r1 = func1(overlaySwapChain);     // 返回 CLegacySwapChain 对象

            // 从 CLegacySwapChain 获取虚函数表，调用索引 19 的函数 CLegacySwapChainBuffer::GetD3D11Resource
            void** vt2 = *(void***)r1;
            VirtFunc func2 = (VirtFunc)vt2[19];     // 索引 19
            void* r2 = func2(r1);                   // 返回纹理对象（IUnknown*）

            // AddRef 增加一次引用计数
            ID3D11Texture2D* tex = nullptr;
            HRESULT hr = ((IUnknown*)r2)->QueryInterface(IID_ID3D11Texture2D, (void**)&tex);
            if (FAILED(hr) || !tex) return nullptr;

            // 验证纹理是否合法
            D3D11_TEXTURE2D_DESC d;
            tex->GetDesc(&d);
            if (d.Width < 64 || d.Height < 64 || d.Width > 16384 || d.Height > 16384 || !(d.BindFlags & D3D11_BIND_RENDER_TARGET))
            {
                tex->Release();
                return nullptr;
            }

            return tex;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            return nullptr;
        }
    }
    default: return nullptr;
    }
    return nullptr;
}

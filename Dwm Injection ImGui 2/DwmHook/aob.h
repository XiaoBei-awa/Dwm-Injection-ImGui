#pragma once

const unsigned char COverlayContext_Present_bytes_win10[] = {
	0x48, 0x89, 0x5c, 0x24, 0x08, 0x48, 0x89, 0x74, 0x24, 0x10, 0x57, 0x48, 0x83, 0xec, 0x40, 0x48, 0x8b, 0xb1, 0x20,
	0x2c, 0x00, 0x00, 0x45, 0x8b, 0xd0, 0x48, 0x8b, 0xfa, 0x48, 0x8b, 0xd9, 0x48, 0x85, 0xf6, 0x0f, 0x85
};

const unsigned char COverlayContext_Present_bytes_w11_21h2[] = {
	0x48, 0x33, 0xC4, 0x48, 0x89, 0x44, 0x24, 0x50, 0x48, 0x8B, 0xB1, 0xA0, 0x2B, 0x00, 0x00, 0x48, 0x8B, 0xFA,
	0x48, 0x8B, 0xD9, 0x48, 0x85, 0xF6
};

const unsigned char COverlayContext_Present_bytes_w11_22h2[] = {
	0x40, 0x53, 0x55, 0x56, 0x57, 0x41, 0x56, 0x41, 0x57, 0x48, 0x81, 0xEC, 0x88, 0x00, 0x00, 0x00,
	0x48, 0x8B, 0x05, '?', '?', '?', '?', 0x48, 0x33, 0xC4, 0x48, 0x89, 0x44, 0x24, 0x78, 0x48
};

const unsigned char COverlayContext_Present_bytes_w11_24h2[] = {
	0x4C, 0x8B, 0xDC, 0x56, 0x41, 0x56
};

const unsigned char COverlayContext_Present_bytes_w11_25h2[] = {
	0x40, 0x55, 0x53, 0x56, 0x57, 0x41, 0x54, 0x41, 0x55, 0x41, 0x56, 0x41, 0x57, 0x48, 0x8D, 0x6C,
	0x24, 0xF9, 0x48, 0x81, 0xEC, 0xF8, 0x00, 0x00, 0x00, 0x48, 0x8B, 0x05,
	'?', '?', '?', '?',
	0x48, 0x33, 0xC4, 0x48, 0x89, 0x45, 0xEF, 0x4C, 0x8B, 0x65,
	'?',
	0x48, 0x8B, 0xD9
};

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

PVOID Get_COverlayContext_Present_Adr(MODULEINFO moduleInfo)
{
    const auto base = (unsigned char*)moduleInfo.lpBaseOfDll;
    const auto size = moduleInfo.SizeOfImage;

    switch (g_WinVersion) 
    {
    case WIN10: 
    {
        for (size_t i = 0; i <= size - sizeof(COverlayContext_Present_bytes_win10); ++i) 
        {
            auto addr = base + i;
            if (memcmp(addr, COverlayContext_Present_bytes_win10, sizeof(COverlayContext_Present_bytes_win10)) == 0)
                return addr;
        }
        break;
    }
    case WIN11_21H2: 
    {
        for (size_t i = 0; i <= size - sizeof(COverlayContext_Present_bytes_w11_21h2); ++i) 
        {
            auto addr = base + i;
            if (memcmp(addr, COverlayContext_Present_bytes_w11_21h2, sizeof(COverlayContext_Present_bytes_w11_21h2)) == 0)
                return addr - 0xF;
        }
        break;
    }
    case WIN11_22H2_23H2: 
    {
        const auto& pattern = COverlayContext_Present_bytes_w11_22h2;

        for (size_t i = 0; i <= size - sizeof(pattern); ++i)
        {
            auto addr = base + i;
            if (!aob_match_inverse(addr, pattern, sizeof(pattern)))
                return addr;
        }
        break;
    }
    case WIN11_24H2: 
    {
        const auto& pattern = COverlayContext_Present_bytes_w11_24h2;

        for (size_t i = 0; i <= size - sizeof(pattern); ++i)
        {
            auto addr = base + i;
            if (!aob_match_inverse(addr, pattern, sizeof(pattern)))
                return addr;
        }
        break;
    }
    case WIN11_25H2: 
    {
        const auto& pattern = COverlayContext_Present_bytes_w11_25h2;

        for (size_t i = 0; i <= size - sizeof(pattern); ++i)
        {
            auto addr = base + i;
            if (!aob_match_inverse(addr, pattern, sizeof(pattern)))
                return addr;
        }
        break;
    }

    default: return nullptr;
    }

    return nullptr;
}
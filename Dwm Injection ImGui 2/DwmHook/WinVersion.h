#pragma once
#pragma comment(lib, "ntdll.lib")

extern "C" NTSYSAPI NTSTATUS NTAPI RtlGetVersion(PRTL_OSVERSIONINFOEXW lpVersionInformation);

typedef enum WIN_VERSION_BUILDNUMBER {
	NTOS_WIN10_22H2 = 19045,
	NTOS_WIN11_21H2 = 22000,
	NTOS_WIN11_22H2 = 22621,
	NTOS_WIN11_23H2 = 22631,
	NTOS_WIN11_24H2 = 26100,
	NTOS_WIN11_25H2 = 26200,
};

enum WinSubVersion {
	UNKNOWN = 0,
	WIN10,
	WIN11_21H2,
	WIN11_22H2_23H2,
	WIN11_24H2,
	WIN11_25H2
};

WinSubVersion g_WinVersion;

void GetWinVersion()
{
	OSVERSIONINFOEXW osvi{};
	osvi.dwOSVersionInfoSize = sizeof(osvi);

	RtlGetVersion(&osvi);

	if (osvi.dwBuildNumber <= NTOS_WIN10_22H2)
		g_WinVersion = WIN10;

	else if (osvi.dwBuildNumber == NTOS_WIN11_21H2)
		g_WinVersion = WIN11_21H2;

	else if (osvi.dwBuildNumber == NTOS_WIN11_22H2 || osvi.dwBuildNumber == NTOS_WIN11_23H2)
		g_WinVersion = WIN11_22H2_23H2;

	else if (osvi.dwBuildNumber == NTOS_WIN11_24H2)
		g_WinVersion = WIN11_24H2;

	else if (osvi.dwBuildNumber == NTOS_WIN11_25H2)
		g_WinVersion = WIN11_25H2;

	else 
	{
		g_WinVersion = UNKNOWN;
		log("[-] dwBuildNumber = %d\n", osvi.dwBuildNumber);
	}
}
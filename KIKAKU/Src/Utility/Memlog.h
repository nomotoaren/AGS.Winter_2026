#pragma once
#include <windows.h>
#include <psapi.h>
#include <cstdio>
#include <DxLib.h>
#pragma comment(lib, "psapi.lib")

// メモリ増加の調査用(確認が終わったら、呼び出しごと消してよい)
// 出力ウィンドウに、プロセスのメモリ量(MB)と画像ハンドルの数を出す
inline void LogMem(const char* tag)
{
	PROCESS_MEMORY_COUNTERS_EX pmc = {};
	pmc.cb = sizeof(pmc);
	GetProcessMemoryInfo(GetCurrentProcess(),
		(PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc));

	char buf[160];
	sprintf_s(buf, "[MEM] %-24s %8.1f MB  graph:%d\n",
		tag, pmc.PrivateUsage / 1048576.0, GetGraphNum());
	OutputDebugStringA(buf);

	OutputDebugStringA(buf);

	FILE* fp = nullptr;
	if (fopen_s(&fp, "mem_log.txt", "a") == 0 && fp != nullptr)
	{
		fputs(buf, fp);
		fclose(fp);
	}
}
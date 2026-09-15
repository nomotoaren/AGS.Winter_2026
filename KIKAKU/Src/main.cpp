#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#include <memory>
#include <string>
#include <DxLib.h>
#include "Application.h"

#ifdef _DEBUG
#define	new	new(_NORMAL_BLOCK, __FILE__, __LINE__)
#endif

std::shared_ptr<int> testSha_;
void TestShared(std::shared_ptr<int> a)
{
	int cnt2 = testSha_.use_count();
	std::shared_ptr<int> b = a;

	// シェアードポインタの参照カウント
	int cnt3 = testSha_.use_count();
}

// WinMain関数
//---------------------------------
int WINAPI WinMain(
	_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, 
	_In_ LPSTR lpCmdLine, _In_ int nCmdShow)
{

	// メモリリーク検出
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);

	// ①生ポインタ
	//int* test = new int();
	//delete test;

	// ②ユニークポインタ
	//std::unique_ptr<int> testI = std::make_unique<int>();
	//std::unique_ptr<std::string> testS = std::make_unique<std::string>();

	//// ③シェアードポインタ
	//testSha_ = std::make_shared<int>(5);
	//// シェアードポインタの参照カウント
	//int cnt1 = testSha_.use_count();
	//// 最初は実装しない
	//TestShared(testSha_);
	//int cnt4 = testSha_.use_count();

	// インスタンスの生成
	Application::CreateInstance();

	// インスタンスの取得
	Application& instance = Application::GetInstance();

	if (instance.IsInitFail())
	{
		// 初期化失敗
		return -1;
	}

	// 実行
	instance.Run();

	// 解放
	instance.Destroy();

	return 0;

}

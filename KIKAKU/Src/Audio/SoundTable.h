#pragma once
#include <string>
#include <unordered_map>

// サウンドID
enum class SoundID
{
	NONE = -1,	// 未再生・無効値

	BGM_TITLE,
	BGM_GAME,
	BGM_TUTORIAL,

	SE_MENU_DECISION,
	SE_MENU_CHOICE,
	SE_MENU_OPEN,

	SE_HIT_LIGHT,
	SE_HIT_MEDIUM,
	SE_HIT_FINISH,
	SE_GUARD,
	SE_KI_HIT,
	SE_BEAM_TICK,

	SE_WALK,
	SE_HAVE_IRON,
	SE_HAVE_WOOD,
	SE_PUT_WOOD,
	SE_BROKE,
	SE_WARNING,
	SE_SUCCESS,

	SE_GAME_CLEAR,

	SE_GAME_OVER,
};

// 読み込むシーン
enum class LoadScene
{
	SYSTEM,			// システム
	TITLE,			// タイトル
	GAME,			// ゲーム
	GAME_CLEAR,		// ゲームクリア
	GAME_OVER,		// ゲームオーバー
	GAME_TUTORIAL,	// チュートリアル
};

// サウンドID → ファイルパス
using SoundPathTable = std::unordered_map<SoundID, std::string>;

// 指定シーンのサウンドテーブルを取得（存在しなければ nullptr）
const SoundPathTable* GetSoundTable(LoadScene scene);
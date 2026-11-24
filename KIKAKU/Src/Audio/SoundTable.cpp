#include "SoundTable.h"

namespace
{
	// システム用
	const SoundPathTable kSystemTable =
	{
		{ SoundID::SE_MENU_DECISION, "Data/Sound/SE/Menu_Decision_SE.mp3" },
		{ SoundID::SE_MENU_CHOICE,   "Data/Sound/SE/Menu_Choice_SE.mp3" },
		{ SoundID::SE_MENU_OPEN,     "Data/Sound/SE/Menu_Open_SE.mp3" },
	};

	// タイトル画面用
	const SoundPathTable kTitleTable =
	{
	};

	// ゲーム画面用
	const SoundPathTable kGameTable =
	{
		{ SoundID::BGM_GAME,         "Data/Sound/BGM/GameScene_BGM.mp3" },
		{ SoundID::SE_WALK,          "Data/Sound/SE/Walk_SE.mp3" },
		{ SoundID::SE_HAVE_IRON,     "Data/Sound/SE/Have_Iron_SE.mp3" },
		{ SoundID::SE_HAVE_WOOD,     "Data/Sound/SE/Have_Wood_SE.mp3" },
		{ SoundID::SE_PUT_WOOD,      "Data/Sound/SE/Put_Wood_SE.mp3" },
		{ SoundID::SE_BROKE,         "Data/Sound/SE/Broke_SE.mp3" },
		{ SoundID::SE_WARNING,       "Data/Sound/SE/Warning_SE.mp3" },
		{ SoundID::SE_SUCCESS,       "Data/Sound/SE/Success_SE.mp3" },
		{ SoundID::SE_HIT_LIGHT,     "Data/Sound/SE/punch_light.mp3" },
		{ SoundID::SE_HIT_MEDIUM,	 "Data/Sound/SE/punch_mid.mp3" },
		{ SoundID::SE_HIT_FINISH,	 "Data/Sound/SE/punch_heavy.mp3" },
		{ SoundID::SE_GUARD,         "Data/Sound/SE/Guard_SE.mp3" },
		{ SoundID::SE_KI_HIT,        "Data/Sound/SE/Ki_Hit_SE.mp3" },
		{ SoundID::SE_BEAM_TICK,     "Data/Sound/SE/Beam_Tick_SE.mp3" },
	};

	// ゲームクリア用
	const SoundPathTable kGameClearTable =
	{
		{ SoundID::SE_GAME_CLEAR, "Data/Sound/SE/Game_Clear_SE.mp3" },
	};

	// ゲームオーバー用
	const SoundPathTable kGameOverTable =
	{
		{ SoundID::SE_GAME_OVER, "Data/Sound/SE/GameOver_SE.mp3" },
	};

}

const SoundPathTable* GetSoundTable(LoadScene scene)
{
	switch (scene)
	{
	case LoadScene::SYSTEM:        return &kSystemTable;
	case LoadScene::TITLE:         return &kTitleTable;
	case LoadScene::GAME:          return &kGameTable;
	case LoadScene::GAME_CLEAR:    return &kGameClearTable;
	case LoadScene::GAME_OVER:     return &kGameOverTable;
	}
	return nullptr;
}
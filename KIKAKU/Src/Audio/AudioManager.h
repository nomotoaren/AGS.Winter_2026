#pragma once

#include <unordered_map>
#include <unordered_set>
#include "SoundTable.h"

class AudioManager
{
public:
	// シングルトン（生成・取得・削除）
	static void CreateInstance(void);
	static AudioManager* GetInstance(void) { return instance_; }
	static void DeleteInstance(void);

	// 初期化
	void Init(void);

	// 指定したシーンのサウンドテーブルを読み込む / 削除する
	void LoadSceneSound(LoadScene scene);
	void DeleteSceneSound(LoadScene scene);

	void PlayBGM(SoundID id);	// BGMを再生
	void StopBGM(void);			// BGMを止める
	void PlaySE(SoundID id);	// SEを再生
	void LoopSE(SoundID id);	// SEをループ再生
	void StopSE(void);			// ループSEを止める
	void DeleteAll(void);		// 全てのサウンドを削除

	// 音量設定（0〜255）
	void SetBgmVolume(int volume);
	void SetSeVolume(int volume);
	void SetMasterVolume(int volume);

	int GetBgmVolume(void) const { return bgmVolume_; }
	int GetSeVolume(void) const { return seVolume_; }
	int GetMasterVolume(void) const { return masterVolume_; }

private:
	AudioManager(void);
	~AudioManager(void);

	// コピー・ムーブ禁止
	AudioManager(const AudioManager&) = delete;
	AudioManager& operator=(const AudioManager&) = delete;
	AudioManager(AudioManager&&) = delete;
	AudioManager& operator=(AudioManager&&) = delete;

	// 読み込み済みサウンド
	struct SoundEntry
	{
		int handle = -1;	// DxLibのサウンドハンドル
		int refCount = 0;	// このサウンドを使っているシーン数
	};

	int  FindHandle(SoundID id) const;						// 未読み込みなら -1
	int  CalcVolume(int categoryVolume) const;				// マスター音量を掛けた実音量
	void ApplyVolume(SoundID id, int categoryVolume);		// 再生中サウンドへ音量反映
	void StartLoop(SoundID id, SoundID& current, int categoryVolume);
	void StopLoop(SoundID& current);
	void ReleaseSound(SoundID id);							// 参照を1つ減らし、0なら削除

	static constexpr int kMaxVolume = 255;
	static AudioManager* instance_;

	std::unordered_map<SoundID, SoundEntry> sounds_;	// ハンドル管理用
	std::unordered_set<LoadScene> loadedScenes_;		// 読み込み済みシーン

	SoundID currentBgm_ = SoundID::NONE;	// 現在再生されているBGM
	SoundID currentLoopSe_ = SoundID::NONE;	// 現在のループSE

	int bgmVolume_ = kMaxVolume;
	int seVolume_ = kMaxVolume;
	int masterVolume_ = kMaxVolume;
};
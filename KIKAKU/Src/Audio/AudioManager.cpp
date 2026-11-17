#include "AudioManager.h"

#include <DxLib.h>
#include <algorithm>

AudioManager* AudioManager::instance_ = nullptr;

// シングルトン
void AudioManager::CreateInstance(void)
{
	if (instance_ == nullptr) instance_ = new AudioManager();
}

void AudioManager::DeleteInstance(void)
{
	delete instance_;
	instance_ = nullptr;
}

AudioManager::AudioManager(void) = default;

AudioManager::~AudioManager(void)
{
	// 念のため全削除
	DeleteAll();
}

void AudioManager::Init(void)
{
	currentBgm_ = SoundID::NONE;
	currentLoopSe_ = SoundID::NONE;

	bgmVolume_ = kMaxVolume;
	seVolume_ = kMaxVolume;
	masterVolume_ = kMaxVolume;
}

// ---------------------------------------------------------------
// 読み込み・削除
// ---------------------------------------------------------------
void AudioManager::LoadSceneSound(LoadScene scene)
{
	const SoundPathTable* table = GetSoundTable(scene);
	if (!table) return;

	// 同じシーンの二重読み込みは無視
	if (!loadedScenes_.insert(scene).second) return;

	for (const auto& [id, path] : *table)
	{
		// 他シーンで読み込み済みなら参照を増やすだけ
		auto it = sounds_.find(id);
		if (it != sounds_.end())
		{
			++it->second.refCount;
			continue;
		}

		const int handle = LoadSoundMem(path.c_str());
		if (handle == -1)
		{
			// 読み込み失敗
			OutputDebugStringA(("AudioManagerサウンドの読み込みに失敗: " + path + "\n").c_str());
			continue;
		}

		sounds_.emplace(id, SoundEntry{ handle, 1 });
	}
}

void AudioManager::DeleteSceneSound(LoadScene scene)
{
	const SoundPathTable* table = GetSoundTable(scene);
	if (!table) return;

	// 読み込んでいないシーンは処理しない
	if (loadedScenes_.erase(scene) == 0) return;

	for (const auto& [id, _] : *table)
	{
		ReleaseSound(id);
	}
}

void AudioManager::DeleteAll(void)
{
	for (const auto& [id, entry] : sounds_)
	{
		DeleteSoundMem(entry.handle);
	}

	sounds_.clear();
	loadedScenes_.clear();
	currentBgm_ = SoundID::NONE;
	currentLoopSe_ = SoundID::NONE;
}

// ---------------------------------------------------------------
// 再生・停止
// ---------------------------------------------------------------
void AudioManager::PlayBGM(SoundID id)
{
	StartLoop(id, currentBgm_, bgmVolume_);
}

void AudioManager::StopBGM(void)
{
	StopLoop(currentBgm_);
}

void AudioManager::PlaySE(SoundID id)
{
	const int handle = FindHandle(id);
	if (handle == -1) return;

	ChangeVolumeSoundMem(CalcVolume(seVolume_), handle);
	PlaySoundMem(handle, DX_PLAYTYPE_BACK, TRUE);
}

void AudioManager::LoopSE(SoundID id)
{
	StartLoop(id, currentLoopSe_, seVolume_);
}

void AudioManager::StopSE(void)
{
	StopLoop(currentLoopSe_);
}

// ---------------------------------------------------------------
// 音量
// ---------------------------------------------------------------
void AudioManager::SetBgmVolume(int volume)
{
	bgmVolume_ = std::clamp(volume, 0, kMaxVolume);
	ApplyVolume(currentBgm_, bgmVolume_);
}

void AudioManager::SetSeVolume(int volume)
{
	seVolume_ = std::clamp(volume, 0, kMaxVolume);
	ApplyVolume(currentLoopSe_, seVolume_);
}

void AudioManager::SetMasterVolume(int volume)
{
	masterVolume_ = std::clamp(volume, 0, kMaxVolume);
	ApplyVolume(currentBgm_, bgmVolume_);
	ApplyVolume(currentLoopSe_, seVolume_);
}

// ---------------------------------------------------------------
// 内部処理
// ---------------------------------------------------------------
int AudioManager::FindHandle(SoundID id) const
{
	auto it = sounds_.find(id);
	return (it != sounds_.end()) ? it->second.handle : -1;
}

int AudioManager::CalcVolume(int categoryVolume) const
{
	return categoryVolume * masterVolume_ / kMaxVolume;
}

void AudioManager::ApplyVolume(SoundID id, int categoryVolume)
{
	const int handle = FindHandle(id);
	if (handle == -1) return;

	ChangeVolumeSoundMem(CalcVolume(categoryVolume), handle);
}

void AudioManager::StartLoop(SoundID id, SoundID& current, int categoryVolume)
{
	const int handle = FindHandle(id);
	if (handle == -1) return;

	// 同じサウンドが再生中ならスキップ
	if (current == id && CheckSoundMem(handle) == 1) return;

	// 別のサウンドが再生中なら停止
	StopLoop(current);

	current = id;
	ChangeVolumeSoundMem(CalcVolume(categoryVolume), handle);
	PlaySoundMem(handle, DX_PLAYTYPE_LOOP, TRUE);
}

void AudioManager::StopLoop(SoundID& current)
{
	if (current == SoundID::NONE) return;

	const int handle = FindHandle(current);
	if (handle != -1) StopSoundMem(handle);

	current = SoundID::NONE;
}

void AudioManager::ReleaseSound(SoundID id)
{
	auto it = sounds_.find(id);
	if (it == sounds_.end()) return;

	// まだ他のシーンが使っているなら残す
	if (--it->second.refCount > 0) return;

	// 再生中のものを削除するなら状態もリセット
	if (currentBgm_ == id) StopBGM();
	if (currentLoopSe_ == id) StopSE();

	DeleteSoundMem(it->second.handle);
	sounds_.erase(it);
}
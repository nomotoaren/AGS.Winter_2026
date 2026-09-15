#pragma once
#include <DxLib.h>
#include <EffekseerForDXLib.h>

class EffekseerEffect
{
public:
    // シングルトン（生成・取得・削除）
    static void CreateInstance(void) { if (instance_ == nullptr) { instance_ = new EffekseerEffect(); } };
    static EffekseerEffect* GetInstance(void) { return instance_; };
    static void DeleteInstance(void) { if (instance_ != nullptr) { delete instance_; instance_ = nullptr; } }

private:
    // 静的インスタンス
    static EffekseerEffect* instance_;
private:
    // デフォルトコンストラクタをprivateにして、外部から生成できない様にする
    EffekseerEffect(void);
    // デストラクタも同様
    ~EffekseerEffect(void);

    // コピー・ムーブ操作を禁止
    EffekseerEffect(const EffekseerEffect&) = delete;
    EffekseerEffect& operator=(const EffekseerEffect&) = delete;
    EffekseerEffect(EffekseerEffect&&) = delete;
    EffekseerEffect& operator=(EffekseerEffect&&) = delete;

public:

    enum class ComboStep
    {
        First,      // 1段目
        Second,     // 2段目
        Third,      // 3段目
        Finish      // フィニッシュ
    };

    // 初期化
    void Init(void);

    // 更新
    void Update(void);

    // 描画
    void Draw(void);

    // 解放
    void Release(void);

    // デリート
    void Delete(void);

    // チュートリアルエフェクト
    void PlayTutorialEffect(const VECTOR& pos, float rotY);

    void PlayComboEffect(const VECTOR& pos, float rotY);

    void PlayComboEffect2(const VECTOR& pos, float rotY);

    void PlayComboEffect3(const VECTOR& pos, float rotY);

    void PlayComboEffect4(const VECTOR& pos, float rotY);

    void PlayComboEffect5(const VECTOR& pos, float rotY);

    void PlayComboEffect6(const VECTOR& pos, float rotY);

    void PlayHitEffect(
        const VECTOR& pos,
        float rotY
    );

private:
    int shalshutEffectId_;

    int PlayshalshuEffectHandle;

    int slashHandle_ = -1;

    bool isSlashing_ = false;

    // チュートリアルのエフェクト
    //-------------------------------
    int tutorialEffectId_;
    int playTutorialHandle;
    //-------------------------------

    int finisyuId;

    int finisyu2Id;

    int finisyu3Id;

    int finisyu4Id;

    int finisyu5Id;

    int finisyu6Id;

    int playFinisyuHandle;

    int playFinisyu2Handle;

    int playFinisyu3Handle;

    int playFinisyu4Handle;

    int playFinisyu5Handle;

    int playFinisyu6Handle;

    int hitEffectId_;
    int playHitEffectHandle_;

};

#include <DxLib.h>
#include <cmath>
#include "../../Application.h"
#include "../../Manager/ResourceManager.h"
#include "BreakEffect.h"

namespace
{
	// 画像の表示幅(画面の横幅に対する割合)
	constexpr float kWidthRate = 0.34f;

	// --- 演出のタイミング(秒) ---
	constexpr float kPopTime = 0.12f;		// 拡大しながら出る時間
	constexpr float kShatterTime = 0.45f;	// 粉々に割れる瞬間
	constexpr float kFlashTime = 0.12f;		// 出た瞬間の白いフラッシュ
	constexpr float kShatterFlash = 0.10f;	// 割れた瞬間の白いフラッシュ
	constexpr float kFadeTime = 0.55f;		// 破片が消えていく時間(最後の何秒か)

	// --- 割れる前の見た目 ---
	constexpr float kPopStartScale = 1.7f;	// 出始めの大きさ(倍)
	constexpr float kShakeStart = 4.0f;		// 震えの強さ(出た直後, px)
	constexpr float kShakeEnd = 12.0f;		// 震えの強さ(割れる直前, px)

	// --- 破片 ---
	constexpr int kCols = 9;				// 横の分割数
	constexpr int kRows = 4;				// 縦の分割数(破片の数は 列×行×2)
	constexpr float kJitter = 0.35f;		// 格子点のズレ(大きいほど不規則な形)
	constexpr float kSpeedMin = 250.0f;		// 飛ぶ速さ(最小, px/秒)
	constexpr float kSpeedMax = 850.0f;		// 飛ぶ速さ(最大, px/秒)
	constexpr float kDrag = 2.2f;			// 空気抵抗(大きいほど早く減速)
	constexpr float kGravity = 900.0f;		// 落ちる力(px/秒の2乗)
	constexpr float kSpinMax = 12.0f;		// 回転の速さ(最大, ラジアン/秒)

	// 状態
	bool g_active = false;
	float g_timer = 0.0f;
	float g_total = 0.0f;
	VECTOR g_pos = { 0.0f, 0.0f, 0.0f };
	int g_img = -1;

	// 乱数っぽい値(-1.0 〜 1.0)。同じ n なら同じ値になる
	float Noise(int n)
	{
		unsigned int x = static_cast<unsigned int>(n);
		x = (x << 13) ^ x;
		x = (x * (x * x * 15731u + 789221u) + 1376312589u) & 0x7fffffffu;
		return 1.0f - static_cast<float>(x) / 1073741824.0f;
	}

	// 0.0 〜 1.0 の乱数
	float Rand01(int n)
	{
		return Noise(n) * 0.5f + 0.5f;
	}

	float Saturate(float v)
	{
		if (v < 0.0f) { return 0.0f; }
		if (v > 1.0f) { return 1.0f; }
		return v;
	}

	VERTEX2D MakeVertex(float x, float y, float u, float v)
	{
		VERTEX2D vt = {};
		vt.pos = VGet(x, y, 0.0f);
		vt.rhw = 1.0f;
		vt.dif = GetColorU8(255, 255, 255, 255);
		vt.u = u;
		vt.v = v;
		return vt;
	}

	// 格子点(i,j)の画像上の位置(u,v)。内側の点だけ少しズラして、破片の形をバラバラにする
	void GridPoint(int i, int j, float& u, float& v)
	{
		u = static_cast<float>(i) / kCols;
		v = static_cast<float>(j) / kRows;

		const bool inner = (i > 0 && i < kCols && j > 0 && j < kRows);
		if (inner)
		{
			u += Noise(i * 131 + j * 17 + 1) * kJitter / kCols;
			v += Noise(i * 71 + j * 29 + 2) * kJitter / kRows;
		}
	}

	// 割れる前: 画像を1枚の四角形として描く
	void DrawWhole(int img, float cx, float cy, float w, float h)
	{
		const float l = cx - w * 0.5f;
		const float r = cx + w * 0.5f;
		const float t = cy - h * 0.5f;
		const float b = cy + h * 0.5f;

		VERTEX2D vt[6]{};
		vt[0] = MakeVertex(l, t, 0.0f, 0.0f);
		vt[1] = MakeVertex(r, t, 1.0f, 0.0f);
		vt[2] = MakeVertex(r, b, 1.0f, 1.0f);
		vt[3] = vt[0];
		vt[4] = vt[2];
		vt[5] = MakeVertex(l, b, 0.0f, 1.0f);

		DrawPolygon2D(vt, 2, img, TRUE);
	}

	// 破片1枚(三角形)を描く
	//   a,b,c    : 三角形の3点(画像上の u,v)
	//   shardId  : 破片ごとの番号(乱数の種)
	void DrawShard(
		int img, const float uv[3][2], int shardId,
		float cx, float cy, float w, float h,
		float flyTime, float fadeBase, float speedScale)
	{
		// 三角形の重心(画像上)
		const float gu = (uv[0][0] + uv[1][0] + uv[2][0]) / 3.0f;
		const float gv = (uv[0][1] + uv[1][1] + uv[2][1]) / 3.0f;

		// 中心から重心への向き(外側へ飛ばす)
		float dx = (gu - 0.5f) * w;
		float dy = (gv - 0.5f) * h;
		float len = sqrtf(dx * dx + dy * dy);
		if (len < 1.0f) { dx = 1.0f; dy = 0.0f; len = 1.0f; }
		dx /= len;
		dy /= len;

		// ランダムな成分を混ぜて、まっすぐ放射状にならないようにする
		dx += Noise(shardId * 7 + 3) * 0.6f;
		dy += Noise(shardId * 7 + 4) * 0.6f - 0.25f;	// 少し上向きにも飛ぶ

		const float speed = (kSpeedMin + (kSpeedMax - kSpeedMin) * Rand01(shardId * 7 + 5)) * speedScale;
		const float spin = Noise(shardId * 7 + 6) * kSpinMax;

		// 位置: 抵抗を受けて減速しながら飛び、重力で落ちていく
		const float k = kDrag;
		const float dist = (1.0f - expf(-k * flyTime)) / k;
		const float ox = dx * speed * dist;
		const float oy = dy * speed * dist + 0.5f * kGravity * speedScale * flyTime * flyTime;

		const float ang = spin * flyTime;
		const float ca = cosf(ang);
		const float sa = sinf(ang);

		// 消え方は破片ごとに少しずらす
		const float fade = Saturate(fadeBase - Rand01(shardId * 7 + 8) * 0.25f);
		const int alpha = static_cast<int>(255.0f * (1.0f - fade));
		if (alpha <= 0)
		{
			return;
		}
		const float shrink = 1.0f - 0.35f * fade;

		// 重心を中心に回転・縮小して、重心を飛ばした位置へ置く
		const float gx = cx + (gu - 0.5f) * w + ox;
		const float gy = cy + (gv - 0.5f) * h + oy;

		VERTEX2D vt[3]{};
		for (int n = 0; n < 3; n++)
		{
			const float lx = (uv[n][0] - gu) * w * shrink;
			const float ly = (uv[n][1] - gv) * h * shrink;
			vt[n] = MakeVertex(
				gx + lx * ca - ly * sa,
				gy + lx * sa + ly * ca,
				uv[n][0], uv[n][1]);
		}

		SetDrawBlendMode(DX_BLENDMODE_ALPHA, alpha);
		DrawPolygon2D(vt, 1, img, TRUE);
	}
}

void BreakEffect::Play(const VECTOR& worldPos, float totalTime)
{
	// 画像はリソースマネージャーから取る(読み込み済みなら同じものが返る)
	g_img = ResourceManager::GetInstance().Load(ResourceManager::SRC::BREAK).handleId_;

	g_active = true;
	g_timer = 0.0f;
	g_total = totalTime;
	g_pos = worldPos;
}

void BreakEffect::Update(float dt)
{
	if (!g_active)
	{
		return;
	}

	g_timer += dt;
	if (g_timer >= g_total)
	{
		g_active = false;
	}
}

bool BreakEffect::IsPlaying(void)
{
	return g_active;
}

void BreakEffect::Draw(void)
{
	if (!g_active)
	{
		return;
	}

	const int sw = Application::SCREEN_SIZE_X;
	const int sh = Application::SCREEN_SIZE_Y;
	const float t = g_timer;

	// --- 白いフラッシュ(出た瞬間と、割れた瞬間) ---
	{
		float rate = 1.0f - Saturate(t / kFlashTime);
		float flash = 150.0f * rate;

		const float shatterRate = 1.0f - Saturate((t - kShatterTime) / kShatterFlash);
		if (t >= kShatterTime && shatterRate > 0.0f)
		{
			flash = 200.0f * shatterRate;
		}

		if (flash > 0.0f)
		{
			SetDrawBlendMode(DX_BLENDMODE_ADD, static_cast<int>(flash));
			DrawBox(0, 0, sw, sh, GetColor(255, 255, 255), TRUE);
			SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
		}
	}

	int imgW = 0;
	int imgH = 0;
	if (g_img == -1 || GetGraphSize(g_img, &imgW, &imgH) != 0)
	{
		// 画像が読めていない: 文字で代わりに出す(登録の間違いに気づけるように)
		DrawExtendString(sw / 2 - 150, sh / 3, 4.0, 4.0, "BREAK!", GetColor(255, 220, 60));
		return;
	}

	// --- 位置: 敵の頭の位置を画面座標にして、はみ出さないよう中に収める ---
	const float drawW = static_cast<float>(sw) * kWidthRate;
	const float drawH = drawW * static_cast<float>(imgH) / static_cast<float>(imgW);

	float cx = static_cast<float>(sw) * 0.5f;
	float cy = static_cast<float>(sh) * 0.35f;

	const VECTOR sp = ConvWorldPosToScreenPos(VAdd(g_pos, VGet(0.0f, 60.0f, 0.0f)));
	if (sp.z > 0.0f && sp.z < 1.0f)
	{
		cx = sp.x;
		cy = sp.y;
	}

	const float marginX = drawW * 0.6f;
	const float marginY = drawH * 0.8f;
	if (cx < marginX) { cx = marginX; }
	if (cx > sw - marginX) { cx = sw - marginX; }
	if (cy < marginY) { cy = marginY; }
	if (cy > sh - marginY) { cy = sh - marginY; }

	if (t < kShatterTime)
	{
		// --- 割れる前: 拡大しながら出て、だんだん強く震える ---
		const float popRate = Saturate(t / kPopTime);
		const float easeOut = 1.0f - (1.0f - popRate) * (1.0f - popRate);
		const float scale = kPopStartScale + (1.0f - kPopStartScale) * easeOut;

		const float shakeRate = Saturate(t / kShatterTime);
		const float amp = kShakeStart + (kShakeEnd - kShakeStart) * shakeRate;

		// 60分の1秒ごとに震える向きを変える
		const int frame = static_cast<int>(t * 60.0f);
		const float shakeX = Noise(frame * 3 + 1) * amp;
		const float shakeY = Noise(frame * 3 + 2) * amp;

		SetDrawBlendMode(DX_BLENDMODE_ALPHA, 255);
		DrawWhole(g_img, cx + shakeX, cy + shakeY, drawW * scale, drawH * scale);
	}
	else
	{
		// --- 割れた後: 無数の破片になって飛び散る ---
		const float flyTime = t - kShatterTime;

		// 消え始めるのは演出の終わりの kFadeTime 秒前から
		const float fadeBase = Saturate((t - (g_total - kFadeTime)) / kFadeTime);

		// 画面サイズに合わせて、飛ぶ距離も変える(1920幅を基準)
		const float speedScale = static_cast<float>(sw) / 1920.0f;

		for (int j = 0; j < kRows; j++)
		{
			for (int i = 0; i < kCols; i++)
			{
				// 格子の4つの角
				float p[4][2]{};
				GridPoint(i, j, p[0][0], p[0][1]);			// 左上
				GridPoint(i + 1, j, p[1][0], p[1][1]);		// 右上
				GridPoint(i + 1, j + 1, p[2][0], p[2][1]);	// 右下
				GridPoint(i, j + 1, p[3][0], p[3][1]);		// 左下

				// 1マスを2つの三角形に分ける(斜めの向きは交互)
				float tri[2][3][2]{};
				const int idx[2][3] =
				{
					{ 0, 1, 2 }, { 0, 2, 3 }
				};
				const int idxAlt[2][3] =
				{ 
					{ 0, 1, 3 }, { 1, 2, 3 }
				};
				const bool alt = ((i + j) % 2 == 1);

				for (int n = 0; n < 2; n++)
				{
					for (int m = 0; m < 3; m++)
					{
						const int k = alt ? idxAlt[n][m] : idx[n][m];
						tri[n][m][0] = p[k][0];
						tri[n][m][1] = p[k][1];
					}
				}

				const int baseId = (j * kCols + i) * 2;
				DrawShard(g_img, tri[0], baseId, cx, cy, drawW, drawH, flyTime, fadeBase, speedScale);
				DrawShard(g_img, tri[1], baseId + 1, cx, cy, drawW, drawH, flyTime, fadeBase, speedScale);
			}
		}
	}

	SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
}
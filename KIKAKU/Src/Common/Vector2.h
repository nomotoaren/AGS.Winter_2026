#pragma once

// 整数の2次元ベクトル(画面上の位置・大きさなど)
class Vector2
{

public:

	int x;
	int y;

	// コンストラクタ(0, 0 で初期化)
	Vector2(void);

	// コンストラクタ(位置を指定)
	Vector2(int vX, int vY);

	// デストラクタ
	~Vector2(void);

};
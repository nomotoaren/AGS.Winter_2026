#pragma once
#include <DxLib.h>

class KiBlast
{
public:

	KiBlast(VECTOR pos, VECTOR dir);
	~KiBlast(void);

	void Update(void);
	void Draw(void);

	bool IsDead(void) const;

	VECTOR GetPos(void) const;
	void Hit(void);

private:

	VECTOR pos_;
	VECTOR dir_;

	float speed_;
	float lifeTime_;

	bool isDead_;
};
#include <math.h>
#include <DxLib.h>
#include "../Utility/AsoUtility.h"
#include "Quaternion.h"

namespace
{
	// 回転がほぼ同じ向き(cosHalfAngle がこれ以上)のときは、球面補間ではなく直線補間にする
	constexpr float kLerpThreshold = 0.99f;
}

//------------------------------------------------------------
// コンストラクタ
//------------------------------------------------------------
Quaternion::Quaternion(void)
{
	w = 1;
	x = y = z = 0;
}

Quaternion::Quaternion(const VECTOR& rad)
{
	*this = Euler(rad.x, rad.y, rad.z);
}

Quaternion::Quaternion(double ww, double wx, double wy, double wz)
{
	w = ww;
	x = wx;
	y = wy;
	z = wz;
}

Quaternion::~Quaternion(void)
{
}

//------------------------------------------------------------
// 作る
//------------------------------------------------------------
Quaternion Quaternion::Euler(const VECTOR& rad)
{
	return Euler(rad.x, rad.y, rad.z);
}

Quaternion Quaternion::Euler(double radX, double radY, double radZ)
{

	Quaternion ret = Quaternion();

	radX = AsoUtility::RadIn2PI(radX);
	radY = AsoUtility::RadIn2PI(radY);
	radZ = AsoUtility::RadIn2PI(radZ);

	// 半分の角度の cos / sin
	const double cosZ = cos(radZ / 2.0f);
	const double sinZ = sin(radZ / 2.0f);
	const double cosX = cos(radX / 2.0f);
	const double sinX = sin(radX / 2.0f);
	const double cosY = cos(radY / 2.0f);
	const double sinY = sin(radY / 2.0f);

	ret.w = cosX * cosY * cosZ + sinX * sinY * sinZ;
	ret.x = sinX * cosY * cosZ + cosX * sinY * sinZ;
	ret.y = cosX * sinY * cosZ - sinX * cosY * sinZ;
	ret.z = cosX * cosY * sinZ - sinX * sinY * cosZ;

	return ret;

}

Quaternion Quaternion::AngleAxis(double rad, VECTOR axis)
{

	// 軸の長さが0のときは、回転なし
	double norm = (double)axis.x * (double)axis.x + (double)axis.y * (double)axis.y + (double)axis.z * (double)axis.z;
	if (norm <= 0.0f)
	{
		return Quaternion();
	}

	// 軸を長さ1にそろえる
	norm = 1.0 / sqrt(norm);
	axis.x = (float)(axis.x * norm);
	axis.y = (float)(axis.y * norm);
	axis.z = (float)(axis.z * norm);

	const double c = cos(0.5f * rad);
	const double s = sin(0.5f * rad);

	Quaternion ret = Quaternion();
	ret.w = c;
	ret.x = s * axis.x;
	ret.y = s * axis.y;
	ret.z = s * axis.z;

	return ret;

}

Quaternion Quaternion::LookRotation(VECTOR dir)
{
	const VECTOR up = { 0.0f, 1.0f, 0.0f };
	return LookRotation(dir, up);
}

Quaternion Quaternion::LookRotation(VECTOR dir, VECTOR up)
{

	// dir を前、right を右、up を上とする3つの軸を作る
	dir = AsoUtility::VNormalize(dir);
	const VECTOR right = AsoUtility::VNormalize(VCross(up, dir));
	up = VCross(dir, right);

	const auto m00 = right.x;
	const auto m01 = right.y;
	const auto m02 = right.z;
	const auto m10 = up.x;
	const auto m11 = up.y;
	const auto m12 = up.z;
	const auto m20 = dir.x;
	const auto m21 = dir.y;
	const auto m22 = dir.z;

	// 3軸を並べた行列からクォータニオンへ変換する
	// (対角の合計が大きいか、どの軸が一番大きいかで計算式を切り替える)
	auto quaternion = Quaternion();

	const float trace = (m00 + m11) + m22;
	if (trace > 0.0f)
	{
		double num = sqrt(trace + 1.0);
		quaternion.w = num * 0.5;
		num = 0.5 / num;
		quaternion.x = ((double)m12 - m21) * num;
		quaternion.y = ((double)m20 - m02) * num;
		quaternion.z = ((double)m01 - m10) * num;
		return quaternion.Normalized();
	}

	if ((m00 >= m11) && (m00 >= m22))
	{
		const auto num7 = sqrt(((1.0f + m00) - m11) - m22);
		const auto num4 = 0.5f / num7;
		quaternion.x = ((double)m12 - m21) * num4;
		quaternion.y = ((double)m01 + m10) * num4;
		quaternion.z = ((double)m02 + m20) * num4;
		quaternion.w = 0.5 * num7;
		return quaternion.Normalized();
	}

	if (m11 > m22)
	{
		const auto num6 = sqrt(((1.0f + m11) - m00) - m22);
		const auto num3 = 0.5f / num6;
		quaternion.x = ((double)m10 + m01) * num3;
		quaternion.y = 0.5 * num6;
		quaternion.z = ((double)m21 + m12) * num3;
		quaternion.w = ((double)m20 - m02) * num3;
		return quaternion.Normalized();
	}

	const auto num5 = sqrt(((1.0f + m22) - m00) - m11);
	const auto num2 = 0.5f / num5;
	quaternion.x = ((double)m20 + m02) * num2;
	quaternion.y = ((double)m21 + m12) * num2;
	quaternion.z = 0.5 * num5;
	quaternion.w = ((double)m01 - m10) * num2;
	return quaternion.Normalized();

}

Quaternion Quaternion::GetRotation(MATRIX mat)
{

	Quaternion ret;

	const float trace = mat.m[0][0] + mat.m[1][1] + mat.m[2][2] + 1.0f;
	if (trace >= 1.0f)
	{
		const float s = 0.5f / sqrtf(trace);
		ret.w = 0.25f / s;
		ret.x = (mat.m[1][2] - mat.m[2][1]) * s;
		ret.y = (mat.m[2][0] - mat.m[0][2]) * s;
		ret.z = (mat.m[0][1] - mat.m[1][0]) * s;
		return ret;
	}

	// 対角のうち一番大きい成分に合わせて、計算式を切り替える
	const float maxDiag = mat.m[1][1] > mat.m[2][2] ? mat.m[1][1] : mat.m[2][2];

	if (maxDiag < mat.m[0][0])
	{
		float s = sqrtf(mat.m[0][0] - (mat.m[1][1] + mat.m[2][2]) + 1.0f);

		const float xHalf = s * 0.5f;
		s = 0.5f / s;
		ret.x = xHalf;
		ret.y = (mat.m[0][1] + mat.m[1][0]) * s;
		ret.z = (mat.m[2][0] + mat.m[0][2]) * s;
		ret.w = (mat.m[1][2] - mat.m[2][1]) * s;
	}
	else if (maxDiag == mat.m[1][1])
	{
		float s = sqrtf(mat.m[1][1] - (mat.m[2][2] + mat.m[0][0]) + 1.0f);

		const float yHalf = s * 0.5f;
		s = 0.5f / s;
		ret.x = (mat.m[0][1] + mat.m[1][0]) * s;
		ret.y = yHalf;
		ret.z = (mat.m[1][2] + mat.m[2][1]) * s;
		ret.w = (mat.m[2][0] - mat.m[0][2]) * s;
	}
	else
	{
		float s = sqrtf(mat.m[2][2] - (mat.m[0][0] + mat.m[1][1]) + 1.0f);

		const float zHalf = s * 0.5f;
		s = 0.5f / s;
		ret.x = (mat.m[2][0] + mat.m[0][2]) * s;
		ret.y = (mat.m[1][2] + mat.m[2][1]) * s;
		ret.z = zHalf;
		ret.w = (mat.m[0][1] - mat.m[1][0]) * s;
	}

	return ret;

}

Quaternion Quaternion::FromToRotation(VECTOR fromDir, VECTOR toDir)
{

	VECTOR axis = VCross(fromDir, toDir);
	const double angle = AsoUtility::AngleDeg(fromDir, toDir);

	// ほぼ真逆を向いているときは、外積が0に近くて軸が決まらないので、別の軸を探す
	if (angle >= 179.9196)
	{
		const auto r = VCross(fromDir, AsoUtility::DIR_R);
		axis = VCross(r, fromDir);

		const float len = axis.x * axis.x + axis.y * axis.y + axis.z * axis.z;
		if (len < 0.000001f)
		{
			axis = AsoUtility::DIR_U;
		}
	}

	axis = AsoUtility::VNormalize(axis);
	return Quaternion::AngleAxis(AsoUtility::Deg2RadD(angle), axis);

}

Quaternion Quaternion::Identity(void)
{
	return Quaternion(1.0f, 0.0f, 0.0f, 0.0f);
}

//------------------------------------------------------------
// 合成・変換
//------------------------------------------------------------
Quaternion Quaternion::Mult(const Quaternion& q1, const Quaternion& q2)
{

	Quaternion ret = Quaternion();

	// w の計算
	ret.w = q1.w * q2.w - q1.x * q2.x - q1.y * q2.y - q1.z * q2.z;

	// x の計算
	ret.x = q1.w * q2.x + q2.w * q1.x + q1.y * q2.z - q1.z * q2.y;

	// y の計算
	ret.y = q1.w * q2.y + q2.w * q1.y + q1.z * q2.x - q1.x * q2.z;

	// z の計算
	ret.z = q1.w * q2.z + q2.w * q1.z + q1.x * q2.y - q1.y * q2.x;

	return ret;

}

Quaternion Quaternion::Mult(const Quaternion& q) const
{
	return Mult(*this, q);
}

VECTOR Quaternion::PosAxis(const Quaternion& q, VECTOR pos)
{
	// 位置に回転を反映させる
	// pos' = q・pos・q(-1)
	const Quaternion p = Quaternion(0.0f, pos.x, pos.y, pos.z);
	const Quaternion tmp = q.Mult(p).Mult(q.Inverse());
	return { (float)tmp.x, (float)tmp.y, (float)tmp.z };
}

VECTOR Quaternion::PosAxis(VECTOR pos) const
{
	return PosAxis(*this, pos);
}

VECTOR Quaternion::ToEuler(const Quaternion& q)
{

	VECTOR ret{};

	const double r11 = 2 * (q.x * q.z + q.w * q.y);
	const double r12 = q.w * q.w - q.x * q.x - q.y * q.y + q.z * q.z;
	const double r21 = -2 * (q.y * q.z - q.w * q.x);
	const double r31 = 2 * (q.x * q.y + q.w * q.z);
	const double r32 = q.w * q.w - q.x * q.x + q.y * q.y - q.z * q.z;

	ret.x = static_cast<float>(asin(r21));
	ret.y = static_cast<float>(atan2(r11, r12));
	ret.z = static_cast<float>(atan2(r31, r32));

	return ret;

}

VECTOR Quaternion::ToEuler(void) const
{
	return ToEuler(*this);
}

MATRIX Quaternion::ToMatrix(const Quaternion& q)
{

	MATRIX mat{};

	const FLOAT4 fq = { (float)q.x, (float)q.y, (float)q.z, (float)q.w };

	const float sx = fq.x * fq.x * 2.0f;
	const float sy = fq.y * fq.y * 2.0f;
	const float sz = fq.z * fq.z * 2.0f;
	const float cx = fq.y * fq.z * 2.0f;
	const float cy = fq.x * fq.z * 2.0f;
	const float cz = fq.x * fq.y * 2.0f;
	const float wx = fq.w * fq.x * 2.0f;
	const float wy = fq.w * fq.y * 2.0f;
	const float wz = fq.w * fq.z * 2.0f;

	mat.m[0][0] = 1.0f - (sy + sz);	mat.m[0][1] = cz + wz;			mat.m[0][2] = cy - wy;			mat.m[0][3] = 0.0f;
	mat.m[1][0] = cz - wz;			mat.m[1][1] = 1.0f - (sx + sz);	mat.m[1][2] = cx + wx;			mat.m[1][3] = 0.0f;
	mat.m[2][0] = cy + wy;			mat.m[2][1] = cx - wx;			mat.m[2][2] = 1.0f - (sx + sy);	mat.m[2][3] = 0.0f;
	mat.m[3][0] = 0.0f;				mat.m[3][1] = 0.0f;				mat.m[3][2] = 0.0f;				mat.m[3][3] = 1.0f;

	return mat;

}

MATRIX Quaternion::ToMatrix(void) const
{
	return ToMatrix(*this);
}

void Quaternion::ToAngleAxis(float* angle, VECTOR* axis)
{

	// w が1を超えていると acos が計算できないので、先に正規化する
	if (fabs(this->w) > 1.0)
	{
		this->Normalize();
	}
	*angle = 2.0f * acosf((float)this->w);

	// 回転がないときは、角度は0
	if (x == 0 && y == 0 && z == 0)
	{
		*angle = 0.0f;
	}

	const float den = sqrtf(1.0f - (float)(this->w * this->w));
	if (den > 0.0001f)
	{
		const VECTOR v = this->xyz();
		axis->x = v.x / den;
		axis->y = v.y / den;
		axis->z = v.z / den;
	}
	else
	{
		// 角度が0のときは軸が決まらない。どの軸でも問題ないので、X軸にしておく
		*axis = { 1.0f, 0.0f, 0.0f };
	}

}

//------------------------------------------------------------
// 基本ベクトル
//------------------------------------------------------------
VECTOR Quaternion::GetDir(VECTOR dir) const
{
	return PosAxis(dir);
}

VECTOR Quaternion::GetForward(void) const
{
	return GetDir(AsoUtility::DIR_F);
}

VECTOR Quaternion::GetBack(void) const
{
	return GetDir(AsoUtility::DIR_B);
}

VECTOR Quaternion::GetRight(void) const
{
	return GetDir(AsoUtility::DIR_R);
}

VECTOR Quaternion::GetLeft(void) const
{
	return GetDir(AsoUtility::DIR_L);
}

VECTOR Quaternion::GetUp(void) const
{
	return GetDir(AsoUtility::DIR_U);
}

VECTOR Quaternion::GetDown(void) const
{
	return GetDir(AsoUtility::DIR_D);
}

//------------------------------------------------------------
// 計算
//------------------------------------------------------------
double Quaternion::Dot(const Quaternion& q1, const Quaternion& q2)
{
	return (q1.w * q2.w + q1.x * q2.x + q1.y * q2.y + q1.z * q2.z);
}

double Quaternion::Dot(const Quaternion& q) const
{
	return Dot(*this, q);
}

Quaternion Quaternion::Normalize(const Quaternion& q)
{
	const float scale = 1.0f / static_cast<float>(q.Length());
	const VECTOR v = VScale(q.xyz(), scale);
	return Quaternion(q.w * scale, v.x, v.y, v.z);
}

Quaternion Quaternion::Normalized(void) const
{
	Quaternion ret = *this;
	ret.Normalize();
	return ret;
}

void Quaternion::Normalize(void)
{
	const double mag = sqrt(w * w + x * x + y * y + z * z);

	w /= mag;
	x /= mag;
	y /= mag;
	z /= mag;
}

Quaternion Quaternion::Inverse(void) const
{
	// 共役(x, y, z の符号を反転)を、長さの2乗で割る
	const double n = 1.0f / (w * w + x * x + y * y + z * z);
	return Quaternion(w * n, -x * n, -y * n, -z * n);
}

Quaternion Quaternion::Slerp(Quaternion from, Quaternion to, double t)
{
	if (t > 1) t = 1;
	if (t < 0) t = 0;
	return SlerpUnclamped(from, to, (float)t);
}

Quaternion Quaternion::SlerpUnclamped(Quaternion a, Quaternion b, float t)
{

	// どちらかが長さ0のときは、もう片方を返す
	if (a.LengthSquared() == 0.0f)
	{
		if (b.LengthSquared() == 0.0f)
		{
			return Identity();
		}
		return b;
	}
	else if (b.LengthSquared() == 0.0f)
	{
		return a;
	}

	float cosHalfAngle = (float)(a.w * b.w) + VDot(a.xyz(), b.xyz());

	if (cosHalfAngle >= 1.0f || cosHalfAngle <= -1.0f)
	{
		// 角度が0なので、どちらかをそのまま返す
		return a;
	}
	else if (cosHalfAngle < 0.0f)
	{
		// 遠回りになるので、b の符号を反転して近いほうを回る
		b.x = b.x * -1.0f;
		b.y = b.y * -1.0f;
		b.z = b.z * -1.0f;
		b.w = -b.w;
		cosHalfAngle = -cosHalfAngle;
	}

	float blendA;
	float blendB;
	if (cosHalfAngle < kLerpThreshold)
	{
		// 角度が大きいときは、正しい球面補間
		const float halfAngle = acosf(cosHalfAngle);
		const float sinHalfAngle = sinf(halfAngle);
		const float oneOverSinHalfAngle = 1.0f / sinHalfAngle;
		blendA = sinf(halfAngle * (1.0f - t)) * oneOverSinHalfAngle;
		blendB = sinf(halfAngle * t) * oneOverSinHalfAngle;
	}
	else
	{
		// 角度がとても小さいときは、直線補間で十分
		blendA = 1.0f - t;
		blendB = t;
	}

	const VECTOR v = VAdd(VScale(a.xyz(), blendA), VScale(b.xyz(), blendB));
	const Quaternion result = Quaternion(blendA * a.w + blendB * b.w, v.x, v.y, v.z);

	if (result.LengthSquared() > 0.0f)
	{
		return Normalize(result);
	}

	return Identity();

}

Quaternion Quaternion::RotateTowards(const Quaternion& from, const Quaternion& to, float maxDegreesDelta)
{
	const double angle = Quaternion::Angle(from, to);
	if (angle == 0.0)
	{
		return to;
	}

	// 目標までの角度より大きく回さないよう、割合は1まで
	float t = maxDegreesDelta / (float)angle;
	if (t > 1.0f)
	{
		t = 1.0f;
	}

	return Quaternion::SlerpUnclamped(from, to, t);
}

double Quaternion::Angle(const Quaternion& q1, const Quaternion& q2)
{
	// 誤差で内積が1をわずかに超えると acos が計算できない(NaN になる)ので、範囲に収める
	double cosAngle = Quaternion::Dot(q1, q2);
	if (cosAngle > 1.0) { cosAngle = 1.0; }
	if (cosAngle < -1.0) { cosAngle = -1.0; }

	return acos(cosAngle) * (180.0 / DX_PI);
}

//------------------------------------------------------------
// 大きさ
//------------------------------------------------------------
double Quaternion::Length(void) const
{
	return sqrt(x * x + y * y + z * z + w * w);
}

double Quaternion::LengthSquared(void) const
{
	return x * x + y * y + z * z + w * w;
}

VECTOR Quaternion::xyz(void) const
{
	return { (float)x, (float)y, (float)z };
}
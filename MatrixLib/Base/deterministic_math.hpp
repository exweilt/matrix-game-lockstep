#pragma once

#include <d3dx9math.h>
#include <fpm/fixed.hpp>
#include <fpm/math.hpp>

struct FixedMatrix;
using fixed24 = fpm::fixed_24_8;

constexpr fixed24 operator"" _fxd(unsigned long long val) {
    return static_cast<fixed24>(val);
}

constexpr fixed24 operator"" _fxd(long double val) {
    return static_cast<fixed24>(val);
}

struct FixedVector2 {
    fixed24 x;
    fixed24 y;

    // FixedVector2() : x(0), y(0) {}
    FixedVector2(fixed24 x_, fixed24 y_) : x(x_), y(y_) {}

    FixedVector2 operator / (fixed24) const;
    FixedVector2 operator * (fixed24) const;
    // fixed24 operator * (FixedVector3) const;
    FixedVector2 operator + (const FixedVector2 &) const;
    FixedVector2 operator - (const FixedVector2 &) const;

    FixedVector2& operator+=(const FixedVector2& rhs);
    FixedVector2& operator-=(const FixedVector2& rhs);
    FixedVector2& operator*=(fixed24 scalar);

    D3DXVECTOR2 ToD3DX() const;
    FixedVector2 Truncated(fixed24 length_limit) const;
};

struct FixedVector3
{
    fixed24 x;
    fixed24 y;
    fixed24 z;

    FixedVector3(fixed24 _x, fixed24 _y, fixed24 _z) : x(_x), y(_y), z(_z) {};



    FixedVector3 operator / (fixed24) const;
    FixedVector3 operator * (fixed24) const;
    // fixed24 operator * (FixedVector3) const;
    FixedVector3 operator + (const FixedVector3 &) const;
    FixedVector3 operator - (const FixedVector3 &) const;

    FixedVector3& operator+=(const FixedVector3& rhs);
    FixedVector3& operator-=(const FixedVector3& rhs);
    FixedVector3& operator*=(fixed24 scalar);

    FixedVector3 Normalized() const;

    FixedVector3 Cross(const FixedVector3 &b) const;
    fixed24 Dot(const FixedVector3 &b) const;
    fixed24 Length() const;
    fixed24 LengthSq() const;

    D3DXVECTOR3 ToD3DX() const;
    static FixedVector3 FromD3DX(const D3DXVECTOR3& vec);
    FixedVector2 FlatXY() const;

    // D3DXVec3Transform, D3DXVec3TransformCoord, D3DXVec3TransformNormal
    FixedVector3 TransformCoord(FixedMatrix matrix);
    FixedVector3 TransformNormal(FixedMatrix matrix);
};


struct FixedVector4 {
    fixed24 x;
    fixed24 y;
};

struct FixedPlane {
    fixed24 a;
    fixed24 b;
    fixed24 c;
    fixed24 d;

    // D3DXPlaneNormalize

    // D3DXPlaneDotNormal

    static FixedPlane FromPoints(FixedVector3 a1, FixedVector3 a2, FixedVector3 a3);

    FixedVector3 IntersectLine(const FixedVector3 pv1, const FixedVector3 pv2);
    // D3DXPlaneDotCoord

    // D3DXPlaneTransform
};

struct FixedMatrix {
    union {
        struct {
            fixed24        _11, _12, _13, _14;
            fixed24        _21, _22, _23, _24;
            fixed24        _31, _32, _33, _34;
            fixed24        _41, _42, _43, _44;
        };
        fixed24 m[4][4];
    };

    static FixedMatrix GetIdentity();

    FixedMatrix Multiply(const FixedMatrix &other) const;

    FixedMatrix Transposed() const;

    // D3DXMatrixDeterminant
    // fixed24 GetDeterminant() const;

    // D3DXMatrixInverse
    FixedMatrix Inverse() const;

    static FixedMatrix GetRotationX(fixed24 angle);
    static FixedMatrix GetRotationY(fixed24 angle);
    static FixedMatrix GetRotationZ(fixed24 angle);
    static FixedMatrix GetRotationYawPitchRoll(fixed24 yaw, fixed24 pitch, fixed24 roll);

    // D3DXMatrixRotationAxis

    static FixedMatrix GetTranslation(fixed24 x, fixed24 y, fixed24 z);
    static FixedMatrix GetScaling(fixed24 x, fixed24 y, fixed24 z);

    // D3DXMatrixLookAtLH / RH
    static FixedMatrix GetLookingAtLH(FixedVector3 eye, FixedVector3 target, FixedVector3 up);
    static FixedMatrix GetLookingAtRH(FixedVector3 eye, FixedVector3 target, FixedVector3 up);

    // D3DXMatrixPerspectiveFovLH / RH
    static FixedMatrix GetLookingAtLH(fixed24 fovy, fixed24 aspect, fixed24 zn, fixed24 zf);
    static FixedMatrix GetLookingAtRH(fixed24 fovy, fixed24 aspect, fixed24 zn, fixed24 zf);

    // D3DXMatrixOrthoLH / RH
    static FixedMatrix GetOrthoRH(fixed24 w, fixed24 h, fixed24 zn, fixed24 zf);
};

fixed24 fsrnd(fixed24 x);
fixed24 frnd(fixed24 x);
fixed24 frnd(int x);
// fixed24 frnd(fixed24 x);




// #include <d3dx9math.h>

// namespace detmath
// {
//
// constexpr float square(float x) { return x * x; }
// constexpr double square(double x) { return x * x; }
//
// float sqrt(float x);
// double sqrt(double x);
//
// float cos(float x);
// float sin(float x);
// float tan(float x);
//
//
// D3DXVECTOR3 normalize(D3DXVECTOR3 &v);
//             length
//             dot
//             cross
// };
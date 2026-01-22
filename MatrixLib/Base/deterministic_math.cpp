//
// Created by Exweilt on 06/09/2025.
//

#include "deterministic_math.hpp"


FixedVector3 FixedVector3::operator/(const fixed24 x) const
{
    FixedVector3 result = *this;
    result.x /= x;
    result.y /= x;
    result.z /= x;
    return result;
}

FixedVector3 FixedVector3::operator*(const fixed24 x) const
{
    FixedVector3 result = *this;
    result.x *= x;
    result.y *= x;
    result.z *= x;
    return result;
}

// fixed24 FixedVector3::operator*(FixedVector3 v) const
// {
//     FixedVector3 result = v;
//     result.x *= this;
//     result.y *= x;
//     result.z *= x;
//     return result;
// }

FixedVector3 FixedVector3::operator+(const FixedVector3 &v) const
{
    FixedVector3 result = *this;
    result.x += v.x;
    result.y += v.y;
    result.z += v.z;
    return result;
}

FixedVector3 FixedVector3::operator-(const FixedVector3 &v) const
{
    FixedVector3 result = *this;
    result.x -= v.x;
    result.y -= v.y;
    result.z -= v.z;
    return result;
}

FixedVector3 FixedVector3::Normalized() const
{
    fixed24 length = fpm::sqrt(this->x*this->x + this->y*this->y + this->z*this->z);
    return (*this) / length;
}

FixedVector3 FixedVector3::Cross(const FixedVector3 &b) const
{
    FixedVector3 result;
    result.x = this->y * b.z - this->z * b.y;
    result.y = this->z * b.x - this->x * b.z;
    result.z = this->x * b.y - this->y * b.x;
    return result;
}

fixed24 FixedVector3::Dot(const FixedVector3 &b) const
{
    return this->x*b.x + this->y*b.y + this->z*b.z;
}

fixed24 FixedVector3::Length() const
{
}

fixed24 FixedVector3::LengthSq() const
{
}

D3DXVECTOR3 FixedVector3::ToD3DX() const
{
    D3DXVECTOR3 result;
    result.x = static_cast<float>(this->x);
    result.y = static_cast<float>(this->y);
    result.z = static_cast<float>(this->z);
    return result;
}

FixedVector3 FixedVector3::FromD3DX(const D3DXVECTOR3& vec)
{
    FixedVector3 result;
    result.x = static_cast<fixed24>(vec.x);
    result.y = static_cast<fixed24>(vec.y);
    result.z = static_cast<fixed24>(vec.z);
    return result;
}

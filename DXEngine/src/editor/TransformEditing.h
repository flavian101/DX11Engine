#pragma once
#include "scene/Components.h"
#include <cmath>

namespace DXEngine
{
    // ==================================================================
    // Pure maths of editing a transform (no GPU, no UI): tested in editor_tests.
    // ==================================================================
    namespace TransformEditing
    {
        // Euler angles in DEGREES as (pitch about X, yaw about Y, roll about Z): the convention of
        // TransformComponent::SetEulerDegrees (XMQuaternionRotationRollPitchYaw).
        inline DirectX::XMFLOAT4 EulerDegreesToQuaternion(const DirectX::XMFLOAT3& degrees)
        {
            using namespace DirectX;
            XMFLOAT4 q;
            XMStoreFloat4(&q, XMQuaternionRotationRollPitchYaw(
                XMConvertToRadians(degrees.x), XMConvertToRadians(degrees.y), XMConvertToRadians(degrees.z)));
            return q;
        }

        // The angles that produce 'q', with pitch in [-90, 90]. (Several angle triples give the same rotation; this is the
        // canonical one. At pitch = +-90 degrees roll is folded into yaw's twin, as every Euler conversion must.)
        inline DirectX::XMFLOAT3 QuaternionToEulerDegrees(const DirectX::XMFLOAT4& q)
        {
            using namespace DirectX;
            const float xx = q.x * q.x, yy = q.y * q.y, zz = q.z * q.z;

            const float m31 = 2.0f * q.x * q.z + 2.0f * q.y * q.w;
            const float m32 = 2.0f * q.y * q.z - 2.0f * q.x * q.w;
            const float m33 = 1.0f - 2.0f * xx - 2.0f * yy;

            const float cy = std::sqrt(m33 * m33 + m31 * m31);
            const float pitch = std::atan2(-m32, cy);

            float yaw, roll;
            if (cy > 16.0f * 1.1920929e-7f)
            {
                const float m12 = 2.0f * q.x * q.y + 2.0f * q.z * q.w;
                const float m22 = 1.0f - 2.0f * xx - 2.0f * zz;
                yaw  = std::atan2(m31, m33);
                roll = std::atan2(m12, m22);
            }
            else    // looking straight up / down: yaw and roll turn the same axis, put it all in roll
            {
                const float m11 = 1.0f - 2.0f * yy - 2.0f * zz;
                const float m21 = 2.0f * q.x * q.y - 2.0f * q.z * q.w;
                yaw  = 0.0f;
                roll = std::atan2(-m21, m11);
            }
            return { XMConvertToDegrees(pitch), XMConvertToDegrees(yaw), XMConvertToDegrees(roll) };
        }

        // true when two (unit) quaternions are the same rotation (q and -q are)
        inline bool SameRotation(const DirectX::XMFLOAT4& a, const DirectX::XMFLOAT4& b, float tolerance = 1e-5f)
        {
            const float dot = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
            return std::fabs(dot) >= 1.0f - tolerance;
        }

        // ------------------------------------------------------------------
        // EulerCache: the angles an inspector SHOWS for one entity.
        // The transform stores a quaternion, and every rotation has several Euler representations. Converting back every
        // frame makes the fields jump while the user drags (170, 0, 0) turns into (10, 180, 180) the moment it is stored.
        // So the angles the user typed are kept for as long as the stored rotation is still the one they produce, and
        // recomputed from the quaternion only when something else changed it (the gizmo, an animation, a script).
        // Call Reset() when the selection changes.
        // ------------------------------------------------------------------
        class EulerCache
        {
        public:
            DirectX::XMFLOAT3 Get(const DirectX::XMFLOAT4& rotation)
            {
                if (!m_Valid || !SameRotation(EulerDegreesToQuaternion(m_Euler), rotation))
                {
                    m_Euler = QuaternionToEulerDegrees(rotation);
                    m_Valid = true;
                }
                return m_Euler;
            }

            // The user edited the angles: remember them, and return the rotation to store.
            DirectX::XMFLOAT4 Set(const DirectX::XMFLOAT3& degrees)
            {
                m_Euler = degrees;
                m_Valid = true;
                return EulerDegreesToQuaternion(degrees);
            }

            void Reset() { m_Valid = false; }

        private:
            DirectX::XMFLOAT3 m_Euler{ 0, 0, 0 };
            bool m_Valid = false;
        };

        // ------------------------------------------------------------------
        // Uniform scale lock: the user edited ONE component of the scale ('before' -> 'after'); returns the scale where all three
        // moved by the same factor. When the edited component was 0 there is no factor, so all three take its new value.
        // ------------------------------------------------------------------
        inline DirectX::XMFLOAT3 ApplyUniformScaleEdit(const DirectX::XMFLOAT3& before, const DirectX::XMFLOAT3& after)
        {
            const float d[3] = { std::fabs(after.x - before.x), std::fabs(after.y - before.y), std::fabs(after.z - before.z) };
            const int axis = d[0] >= d[1] && d[0] >= d[2] ? 0 : (d[1] >= d[2] ? 1 : 2);
            const float b[3] = { before.x, before.y, before.z };
            const float a[3] = { after.x, after.y, after.z };
            if (d[axis] == 0.0f) return after;                       // nothing changed
            if (std::fabs(b[axis]) < 1e-6f) return { a[axis], a[axis], a[axis] };
            const float factor = a[axis] / b[axis];
            return { before.x * factor, before.y * factor, before.z * factor };
        }

        // ------------------------------------------------------------------
        // The local position / rotation / scale that put an entity at 'world' under a parent at 'parentWorld'
        // (identity for a root): local = world * inverse(parentWorld). Used when reparenting without moving the entity
        // and when a gizmo hands back a new WORLD matrix. Writes position, rotation and scale only.
        // Returns false (and writes nothing) when the parent cannot be inverted or the result cannot be decomposed.
        // A parent with non-uniform scale and a rotated child produce a shear a TRS transform cannot hold; the closest TRS is stored.
        // ------------------------------------------------------------------
        inline bool ComputeLocalFromWorld(const DirectX::XMMATRIX& world, const DirectX::XMMATRIX& parentWorld, TransformComponent& out)
        {
            using namespace DirectX;
            XMVECTOR determinant;
            const XMMATRIX inverse = XMMatrixInverse(&determinant, parentWorld);
            if (std::fabs(XMVectorGetX(determinant)) < 1e-12f)
                return false;

            XMVECTOR scale, rotation, translation;
            if (!XMMatrixDecompose(&scale, &rotation, &translation, world * inverse))
                return false;

            XMStoreFloat3(&out.scale, scale);
            XMStoreFloat4(&out.rotation, XMQuaternionNormalize(rotation));
            XMStoreFloat3(&out.position, translation);
            return true;
        }
    }
}

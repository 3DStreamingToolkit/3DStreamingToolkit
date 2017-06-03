//
// ArcBall.h
//
// Ken Shoemake, "Arcball Rotation Control", Graphics Gems IV, pg 176 - 192  
//

#pragma once

class ArcBall
{
public:
    ArcBall() :
        m_width(800.f),
        m_height(400.f),
        m_radius(1.f),
        m_drag(false) { Reset();  }

    void Reset()
    {
        m_qdown = m_qnow = DirectX::SimpleMath::Quaternion::Identity;
    }

    void OnBegin( int x, int y )
    {
        m_drag = true;
        m_qdown = m_qnow;
        m_downPoint = ScreenToVector(float(x), float(y));
    }

    void OnMove(int x, int y)
    {
        using namespace DirectX;
        if (m_drag)
        {
            XMVECTOR curr = ScreenToVector(float(x), float(y));

            m_qnow = XMQuaternionMultiply(m_qdown, QuatFromBallPoints(m_downPoint, curr));
            m_qnow.Normalize();
        }
    }

    void OnEnd()
    {
        m_drag = false;
    }

    void SetWindow(int width, int height)
    {
        m_width = float(width);
        m_height = float(height);
    }

    void SetRadius(float radius)
    {
        m_radius = radius;
    }

    DirectX::SimpleMath::Quaternion GetQuat() const { return m_qnow; }

    bool IsDragging() const { return m_drag; }

private:
    float                           m_width;
    float                           m_height;
    float                           m_radius;
    DirectX::SimpleMath::Quaternion m_qdown;
    DirectX::SimpleMath::Quaternion m_qnow;
    DirectX::SimpleMath::Vector3    m_downPoint;
    bool                            m_drag;

    DirectX::XMVECTOR ScreenToVector(float screenx, float screeny)
    {
        float x = -( screenx - m_width / 2.f ) / ( m_radius * m_width / 2.f );
        float y = ( screeny - m_height / 2.f ) / ( m_radius * m_height / 2.f );

        float z = 0.0f;
        float mag = x * x + y * y;

        if( mag > 1.0f )
        {
            float scale = 1.0f / sqrtf( mag );
            x *= scale;
            y *= scale;
        }
        else
            z = sqrtf( 1.0f - mag );

        return DirectX::XMVectorSet( x, y, z, 0 );
    }

    static DirectX::XMVECTOR QuatFromBallPoints( DirectX::FXMVECTOR vFrom, DirectX::FXMVECTOR vTo )
    {
        using namespace DirectX;
        XMVECTOR dot = XMVector3Dot( vFrom, vTo );
        XMVECTOR vPart = XMVector3Cross( vFrom, vTo );
        return XMVectorSelect( dot, vPart, g_XMSelect1110 );
    }
};
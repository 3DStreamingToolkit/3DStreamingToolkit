#pragma once

// Eye is at (0, 0, 1), looking at point (0, 0, 0) with the up-vector along the y-axis.
static const DirectX::XMVECTORF32 kEye = { 0.0f, 0.0f, 1.0f, 0.0f };
static const DirectX::XMVECTORF32 kLookAt = { 0.0f, 0.0f, 0.0f, 0.0f };
static const DirectX::XMVECTORF32 kUp = { 0.0f, 1.0f, 0.0f, 0.0f };

namespace StreamingToolkitSample
{
	class CubeRenderer
	{
	public:
											CubeRenderer(int width, int height);

		void								Render();
		void								ToPerspective();
		void								UpdateView(const DirectX::XMVECTORF32& eye, const DirectX::XMVECTORF32& lookAt, const DirectX::XMVECTORF32& up);
		void								SetCamera();

		// Property accessors.
		DirectX::XMVECTORF32				GetDefaultEyeVector() { return kEye; }
		DirectX::XMVECTORF32				GetDefaultLookAtVector() { return kLookAt; }
		DirectX::XMVECTORF32				GetDefaultUpVector() { return kUp; }
	};
}

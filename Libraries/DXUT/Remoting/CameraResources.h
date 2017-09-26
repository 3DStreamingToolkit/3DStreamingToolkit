//--------------------------------------------------------------------------------------
// File: CameraResources.h
//
// Helper functions for Direct3D programming.
//
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//
// http://go.microsoft.com/fwlink/?LinkId=320437
//--------------------------------------------------------------------------------------
#pragma once

//--------------------------------------------------------------------------------------
// Simple stereo camera class that accepts left and right view projection matrices 
// as input.
//--------------------------------------------------------------------------------------
class CameraResources
{
public:
	CameraResources(bool isStereo = false);
	virtual ~CameraResources();

	void SetViewMatrix(const DirectX::XMFLOAT4X4& viewLeft, const DirectX::XMFLOAT4X4& viewRight);
	void SetProjMatrix(const DirectX::XMFLOAT4X4& projLeft, const DirectX::XMFLOAT4X4& projRight);
	void SetViewport(D3D11_VIEWPORT* vp);
	void SetViewport(int width, int height);
	void SetStereo(bool isEnabled);

	DirectX::XMFLOAT4X4* GetProjMatrix();
	DirectX::XMFLOAT4X4* GetViewMatrix();
	D3D11_VIEWPORT* GetViewport();
	bool IsStereo();

private:
	// True for stereo output, false for mono output.
	bool m_isStereo;

	// Viewport.
	D3D11_VIEWPORT* m_pViewport;

	// Left and right projection matrices for stereo output.
	DirectX::XMFLOAT4X4 m_mProjStereo[2];

	// Left and right view matrices for stereo output.
	DirectX::XMFLOAT4X4 m_mViewStereo[2];
};

//--------------------------------------------------------------------------------------
// File: CameraResources.cpp
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
#include "DXUT.h"
#include "CameraResources.h"


using namespace DirectX;


CameraResources::CameraResources(bool isStereo) :
	m_isStereo(isStereo),
	m_pViewport(NULL)
{
}


CameraResources::~CameraResources()
{
	SAFE_DELETE_ARRAY(m_pViewport);
}


void CameraResources::SetViewMatrix(const DirectX::XMFLOAT4X4& viewLeft, const DirectX::XMFLOAT4X4& viewRight)
{
	m_mViewStereo[0] = viewLeft;
	m_mViewStereo[1] = viewRight;
}


void CameraResources::SetProjMatrix(const DirectX::XMFLOAT4X4& projLeft, const DirectX::XMFLOAT4X4& projRight)
{
	m_mProjStereo[0] = projLeft;
	m_mProjStereo[1] = projRight;
}


void CameraResources::SetViewport(D3D11_VIEWPORT* vp)
{
	m_pViewport = vp;
}


void CameraResources::SetViewport(int width, int height)
{
	if (!m_pViewport)
	{
		m_pViewport = new D3D11_VIEWPORT[2];
		m_pViewport[0] = CD3D11_VIEWPORT(
			0.0f,
			0.0f,
			width,
			height);

		m_pViewport[1] = CD3D11_VIEWPORT(
			width,
			0.0f,
			width,
			height);
	}
}


void CameraResources::SetStereo(bool isEnabled)
{
	m_isStereo = isEnabled;
}


XMFLOAT4X4* CameraResources::GetProjMatrix()
{
	return m_mProjStereo;
}


XMFLOAT4X4* CameraResources::GetViewMatrix()
{
	return m_mViewStereo;
}


D3D11_VIEWPORT* CameraResources::GetViewport()
{
	return m_pViewport;
}


bool CameraResources::IsStereo()
{
	return m_isStereo;
}

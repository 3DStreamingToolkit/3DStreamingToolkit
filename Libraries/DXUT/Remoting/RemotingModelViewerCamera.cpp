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
#include "RemotingModelViewerCamera.h"


using namespace DirectX;


RemotingModelViewerCamera::RemotingModelViewerCamera()
{
}


void RemotingModelViewerCamera::SetViewParams(
	const FXMVECTOR& vEyePt,
	const FXMVECTOR& vLookatPt,
	const FXMVECTOR& pvUpPt)
{
	CBaseCamera::SetViewParams(vEyePt, vLookatPt);

	// Propogate changes to the member arcball
	XMMATRIX mRotation = XMMatrixLookAtLH(vEyePt, vLookatPt, pvUpPt);
	XMVECTOR quat = XMQuaternionRotationMatrix(mRotation);
	m_ViewArcBall.SetQuatNow(quat);

	// Set the radius according to the distance
	XMVECTOR vEyeToPoint = XMVectorSubtract(vLookatPt, vEyePt);
	float len = XMVectorGetX(XMVector3Length(vEyeToPoint));
	SetRadius(len);

	// View information changed. FrameMove should be called.
	m_bDragSinceLastUpdate = true;
}

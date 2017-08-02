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

#include "DXUTCamera.h"

//--------------------------------------------------------------------------------------
// A derived class from CModelViewerCamera that processes up vector.
//--------------------------------------------------------------------------------------
class RemotingModelViewerCamera : public CModelViewerCamera
{
public:
	RemotingModelViewerCamera();

	void SetViewParams(const DirectX::FXMVECTOR& vEyePt, const DirectX::FXMVECTOR& vLookatPt, const DirectX::FXMVECTOR& pvUpPt = DirectX::g_XMIdentityR1);
};

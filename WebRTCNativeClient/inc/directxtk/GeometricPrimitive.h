//--------------------------------------------------------------------------------------
// File: GeometricPrimitive.h
//
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//
// http://go.microsoft.com/fwlink/?LinkId=248929
//--------------------------------------------------------------------------------------

#pragma once

#include "VertexTypes.h"

#include <DirectXColors.h>
#include <functional>
#include <memory>
#include <vector>


namespace DirectX
{
    class IEffect;

    class GeometricPrimitive
    {
    public:
        GeometricPrimitive(GeometricPrimitive const&) = delete;
        GeometricPrimitive& operator= (GeometricPrimitive const&) = delete;

        virtual ~GeometricPrimitive();
        
        // Factory methods.
        static std::unique_ptr<GeometricPrimitive> __cdecl CreateCube         (_In_ ID3D11DeviceContext* deviceContext, float size = 1, bool rhcoords = true);
        static std::unique_ptr<GeometricPrimitive> __cdecl CreateBox          (_In_ ID3D11DeviceContext* deviceContext, const XMFLOAT3& size, bool rhcoords = true, bool invertn = false);
        static std::unique_ptr<GeometricPrimitive> __cdecl CreateSphere       (_In_ ID3D11DeviceContext* deviceContext, float diameter = 1, size_t tessellation = 16, bool rhcoords = true, bool invertn = false);
        static std::unique_ptr<GeometricPrimitive> __cdecl CreateGeoSphere    (_In_ ID3D11DeviceContext* deviceContext, float diameter = 1, size_t tessellation = 3, bool rhcoords = true);
        static std::unique_ptr<GeometricPrimitive> __cdecl CreateCylinder     (_In_ ID3D11DeviceContext* deviceContext, float height = 1, float diameter = 1, size_t tessellation = 32, bool rhcoords = true);
        static std::unique_ptr<GeometricPrimitive> __cdecl CreateCone         (_In_ ID3D11DeviceContext* deviceContext, float diameter = 1, float height = 1, size_t tessellation = 32, bool rhcoords = true);
        static std::unique_ptr<GeometricPrimitive> __cdecl CreateTorus        (_In_ ID3D11DeviceContext* deviceContext, float diameter = 1, float thickness = 0.333f, size_t tessellation = 32, bool rhcoords = true);
        static std::unique_ptr<GeometricPrimitive> __cdecl CreateTetrahedron  (_In_ ID3D11DeviceContext* deviceContext, float size = 1, bool rhcoords = true);
        static std::unique_ptr<GeometricPrimitive> __cdecl CreateOctahedron   (_In_ ID3D11DeviceContext* deviceContext, float size = 1, bool rhcoords = true);
        static std::unique_ptr<GeometricPrimitive> __cdecl CreateDodecahedron (_In_ ID3D11DeviceContext* deviceContext, float size = 1, bool rhcoords = true);
        static std::unique_ptr<GeometricPrimitive> __cdecl CreateIcosahedron  (_In_ ID3D11DeviceContext* deviceContext, float size = 1, bool rhcoords = true);
        static std::unique_ptr<GeometricPrimitive> __cdecl CreateTeapot       (_In_ ID3D11DeviceContext* deviceContext, float size = 1, size_t tessellation = 8, bool rhcoords = true);
        static std::unique_ptr<GeometricPrimitive> __cdecl CreateCustom       (_In_ ID3D11DeviceContext* deviceContext, const std::vector<VertexPositionNormalTexture>& vertices, const std::vector<uint16_t>& indices);

        static void __cdecl CreateCube          (std::vector<VertexPositionNormalTexture>& vertices, std::vector<uint16_t>& indices, float size = 1, bool rhcoords = true);
        static void __cdecl CreateBox           (std::vector<VertexPositionNormalTexture>& vertices, std::vector<uint16_t>& indices, const XMFLOAT3& size, bool rhcoords = true, bool invertn = false);
        static void __cdecl CreateSphere        (std::vector<VertexPositionNormalTexture>& vertices, std::vector<uint16_t>& indices, float diameter = 1, size_t tessellation = 16, bool rhcoords = true, bool invertn = false);
        static void __cdecl CreateGeoSphere     (std::vector<VertexPositionNormalTexture>& vertices, std::vector<uint16_t>& indices, float diameter = 1, size_t tessellation = 3, bool rhcoords = true);
        static void __cdecl CreateCylinder      (std::vector<VertexPositionNormalTexture>& vertices, std::vector<uint16_t>& indices, float height = 1, float diameter = 1, size_t tessellation = 32, bool rhcoords = true);
        static void __cdecl CreateCone          (std::vector<VertexPositionNormalTexture>& vertices, std::vector<uint16_t>& indices, float diameter = 1, float height = 1, size_t tessellation = 32, bool rhcoords = true);
        static void __cdecl CreateTorus         (std::vector<VertexPositionNormalTexture>& vertices, std::vector<uint16_t>& indices, float diameter = 1, float thickness = 0.333f, size_t tessellation = 32, bool rhcoords = true);
        static void __cdecl CreateTetrahedron   (std::vector<VertexPositionNormalTexture>& vertices, std::vector<uint16_t>& indices, float size = 1, bool rhcoords = true);
        static void __cdecl CreateOctahedron    (std::vector<VertexPositionNormalTexture>& vertices, std::vector<uint16_t>& indices, float size = 1, bool rhcoords = true);
        static void __cdecl CreateDodecahedron  (std::vector<VertexPositionNormalTexture>& vertices, std::vector<uint16_t>& indices, float size = 1, bool rhcoords = true);
        static void __cdecl CreateIcosahedron   (std::vector<VertexPositionNormalTexture>& vertices, std::vector<uint16_t>& indices, float size = 1, bool rhcoords = true);
        static void __cdecl CreateTeapot        (std::vector<VertexPositionNormalTexture>& vertices, std::vector<uint16_t>& indices, float size = 1, size_t tessellation = 8, bool rhcoords = true);

        // Draw the primitive.
        void XM_CALLCONV Draw(FXMMATRIX world, CXMMATRIX view, CXMMATRIX projection, FXMVECTOR color = Colors::White, _In_opt_ ID3D11ShaderResourceView* texture = nullptr, bool wireframe = false,
                              _In_opt_ std::function<void __cdecl()> setCustomState = nullptr ) const;

        // Draw the primitive using a custom effect.
        void __cdecl Draw( _In_ IEffect* effect, _In_ ID3D11InputLayout* inputLayout, bool alpha = false, bool wireframe = false,
                           _In_opt_ std::function<void __cdecl()> setCustomState = nullptr ) const;

        // Create input layout for drawing with a custom effect.
        void __cdecl CreateInputLayout( _In_ IEffect* effect, _Outptr_ ID3D11InputLayout** inputLayout ) const;
        
    private:
        GeometricPrimitive();

        // Private implementation.
        class Impl;

        std::unique_ptr<Impl> pImpl;
    };
}

//--------------------------------------------------------------------------------------
// File: WICTextureLoader.h
//
// Function for loading a WIC image and creating a Direct3D 11 runtime texture for it
// (auto-generating mipmaps if possible)
//
// Note: Assumes application has already called CoInitializeEx
//
// Warning: CreateWICTexture* functions are not thread-safe if given a d3dContext instance for
//          auto-gen mipmap support.
//
// Note these functions are useful for images created as simple 2D textures. For
// more complex resources, DDSTextureLoader is an excellent light-weight runtime loader.
// For a full-featured DDS file reader, writer, and texture processing pipeline see
// the 'Texconv' sample and the 'DirectXTex' library.
//
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//
// http://go.microsoft.com/fwlink/?LinkId=248926
// http://go.microsoft.com/fwlink/?LinkId=248929
//--------------------------------------------------------------------------------------

#pragma once

#include <d3d11_1.h>
#include <stdint.h>


namespace DirectX
{
    // Standard version
    HRESULT CreateWICTextureFromMemory( _In_ ID3D11Device* d3dDevice,
                                        _In_reads_bytes_(wicDataSize) const uint8_t* wicData,
                                        _In_ size_t wicDataSize,
                                        _Out_opt_ ID3D11Resource** texture,
                                        _Out_opt_ ID3D11ShaderResourceView** textureView,
                                        _In_ size_t maxsize = 0
                                      );

    HRESULT CreateWICTextureFromFile( _In_ ID3D11Device* d3dDevice,
                                      _In_z_ const wchar_t* szFileName,
                                      _Out_opt_ ID3D11Resource** texture,
                                      _Out_opt_ ID3D11ShaderResourceView** textureView,
                                      _In_ size_t maxsize = 0
                                    );

    // Standard version with optional auto-gen mipmap support
    HRESULT CreateWICTextureFromMemory( _In_ ID3D11Device* d3dDevice,
                                        _In_opt_ ID3D11DeviceContext* d3dContext,
                                        _In_reads_bytes_(wicDataSize) const uint8_t* wicData,
                                        _In_ size_t wicDataSize,
                                        _Out_opt_ ID3D11Resource** texture,
                                        _Out_opt_ ID3D11ShaderResourceView** textureView,
                                        _In_ size_t maxsize = 0
                                      );

    HRESULT CreateWICTextureFromFile( _In_ ID3D11Device* d3dDevice,
                                      _In_opt_ ID3D11DeviceContext* d3dContext,
                                      _In_z_ const wchar_t* szFileName,
                                      _Out_opt_ ID3D11Resource** texture,
                                      _Out_opt_ ID3D11ShaderResourceView** textureView,
                                      _In_ size_t maxsize = 0
                                    );

    // Extended version
    HRESULT CreateWICTextureFromMemoryEx( _In_ ID3D11Device* d3dDevice,
                                          _In_reads_bytes_(wicDataSize) const uint8_t* wicData,
                                          _In_ size_t wicDataSize,
                                          _In_ size_t maxsize,
                                          _In_ D3D11_USAGE usage,
                                          _In_ unsigned int bindFlags,
                                          _In_ unsigned int cpuAccessFlags,
                                          _In_ unsigned int miscFlags,
                                          _In_ bool forceSRGB,
                                          _Out_opt_ ID3D11Resource** texture,
                                          _Out_opt_ ID3D11ShaderResourceView** textureView
                                      );

    HRESULT CreateWICTextureFromFileEx( _In_ ID3D11Device* d3dDevice,
                                        _In_z_ const wchar_t* szFileName,
                                        _In_ size_t maxsize,
                                        _In_ D3D11_USAGE usage,
                                        _In_ unsigned int bindFlags,
                                        _In_ unsigned int cpuAccessFlags,
                                        _In_ unsigned int miscFlags,
                                        _In_ bool forceSRGB,
                                        _Out_opt_ ID3D11Resource** texture,
                                        _Out_opt_ ID3D11ShaderResourceView** textureView
                                    );

    // Extended version with optional auto-gen mipmap support
    HRESULT CreateWICTextureFromMemoryEx( _In_ ID3D11Device* d3dDevice,
                                          _In_opt_ ID3D11DeviceContext* d3dContext,
                                          _In_reads_bytes_(wicDataSize) const uint8_t* wicData,
                                          _In_ size_t wicDataSize,
                                          _In_ size_t maxsize,
                                          _In_ D3D11_USAGE usage,
                                          _In_ unsigned int bindFlags,
                                          _In_ unsigned int cpuAccessFlags,
                                          _In_ unsigned int miscFlags,
                                          _In_ bool forceSRGB,
                                          _Out_opt_ ID3D11Resource** texture,
                                          _Out_opt_ ID3D11ShaderResourceView** textureView
                                      );

    HRESULT CreateWICTextureFromFileEx( _In_ ID3D11Device* d3dDevice,
                                        _In_opt_ ID3D11DeviceContext* d3dContext,
                                        _In_z_ const wchar_t* szFileName,
                                        _In_ size_t maxsize,
                                        _In_ D3D11_USAGE usage,
                                        _In_ unsigned int bindFlags,
                                        _In_ unsigned int cpuAccessFlags,
                                        _In_ unsigned int miscFlags,
                                        _In_ bool forceSRGB,
                                        _Out_opt_ ID3D11Resource** texture,
                                        _Out_opt_ ID3D11ShaderResourceView** textureView
                                    );
}
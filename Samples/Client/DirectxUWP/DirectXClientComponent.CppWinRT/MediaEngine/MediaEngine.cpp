//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

#include "MediaEngine.h"

SafeString::SafeString()
    : m_hstring(nullptr)
{
}

SafeString::~SafeString()
{
    if (nullptr != m_hstring)
    {
        WindowsDeleteString(m_hstring);
    }
}

SafeString::operator const HSTRING&() const
{
    return m_hstring;
}

HSTRING* SafeString::GetAddressOf()
{
    return &m_hstring;
}

const wchar_t * SafeString::c_str() const
{
    return WindowsGetStringRawBuffer(m_hstring, nullptr);
}

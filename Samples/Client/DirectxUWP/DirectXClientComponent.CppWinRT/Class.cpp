﻿#include "pch.h"
#include "Class.h"

namespace winrt::DirectXClientComponent_CppWinRT::implementation
{
    int32_t Class::MyProperty()
    {
        throw hresult_not_implemented();
    }

    void Class::MyProperty(int32_t /* value */)
    {
        throw hresult_not_implemented();
    }
}

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

using System;
using Windows.UI.Xaml;
using Windows.UI.Xaml.Data;

namespace PeerConnectionClient.Utilities
{
    /// <summary>
    /// Class to convert a null to a Visibility type.
    /// Implements the IValueConverter.
    /// </summary>
    class NullToVisibleConverter : IValueConverter
    {
        public bool Negated { get; set; }

        /// <summary>
        /// See IValueConverter.Convert().
        /// </summary>
        public object Convert(object value, Type targetType, object parameter, string language)
        {
            return value == null ? Visibility.Collapsed : Visibility.Visible;
        }

        /// <summary>
        /// See IValueConverter.ConvertBack().
        /// </summary>
        public object ConvertBack(object value, Type targetType, object parameter, string language)
        {
            return null;
        }
    }
}

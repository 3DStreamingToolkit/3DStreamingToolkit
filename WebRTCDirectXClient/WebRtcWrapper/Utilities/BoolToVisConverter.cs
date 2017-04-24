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
    /// Class provides functionality to convert from boolean to Visibility.
    /// Implements the IValueConverter interface.
    /// </summary>
    public class BoolToVisConverter : IValueConverter
    {
        /// <summary>
        /// Converts a boolean to it's negated value.
        /// </summary>
        public bool Negated { get; set; }

        /// <summary>
        /// See the IValueConverter.Convert().
        /// </summary>
        public object Convert(object value, Type targetType, object parameter, string language)
        {
            var result = (bool)value;
            result = Negated ? !result : result;
            return result ? Visibility.Visible : Visibility.Collapsed;
        }

        /// <summary>
        /// See the IValueConverter.ConvertBack().
        /// </summary>
        public object ConvertBack(object value, Type targetType, object parameter, string language)
        {
            if (Negated)
            {
                return (bool)value ? Visibility.Collapsed : Visibility.Visible;
            }
            return (bool)value ? Visibility.Visible : Visibility.Collapsed;
        }
    }
}

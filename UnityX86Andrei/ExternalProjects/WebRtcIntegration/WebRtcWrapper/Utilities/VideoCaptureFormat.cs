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

namespace PeerConnectionClient.Utilities
{
    /// <summary>
    /// Enumerable representing video capture resolution.
    /// </summary>
    public enum CapRes
    {
        Default = 0,
        _640_480 = 1,
        _320_240 = 2,
    };
    
    /// <summary>
    /// Enumerable representing video frames per second.
    /// </summary>
    public enum CapFPS
    {
        Default = 0,
        _5 = 1,
        _15 = 2,
        _30 = 3
    };

    /// <summary>
    /// Class representing a combo box item with 
    /// video capture enumerable value.
    /// </summary>
    public class ComboBoxItemCapRes
    {
        public CapRes ValueCapResEnum { get; set; }
        public string ValueCapResString { get; set; }
    }

    /// <summary>
    /// Class representing a combo box item with
    /// video frames per second enumerable value.
    /// </summary>
    public class ComboBoxItemCapFPS
    {
        public CapFPS ValueCapFPSEnum { get; set; }
        public string ValueCapFPSString { get; set; }
    }
}

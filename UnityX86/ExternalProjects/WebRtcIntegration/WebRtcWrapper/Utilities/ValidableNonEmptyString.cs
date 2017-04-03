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
    /// Class to validate that the string member variable is not empty.
    /// </summary>
    public class ValidableNonEmptyString : ValidableBase<string>
    {
        /// <summary>
        /// Default constructor initializes Value with an empty string.
        /// </summary>
        public ValidableNonEmptyString()
        {
            Value = "";
        }

        /// <summary>
        /// Constructor initializes the Value with the string value.
        /// </summary>
        /// <param name="value">String value</param>
        public ValidableNonEmptyString(string value = "")
        {
            Value = value;
        }

        /// <summary>
        /// Overrides the ValidableBase.Validate() method.
        /// Validates the string is not empty.
        /// </summary>
        override protected void Validate()
        {
            Valid = !string.IsNullOrEmpty(Value);
        }
    }
}

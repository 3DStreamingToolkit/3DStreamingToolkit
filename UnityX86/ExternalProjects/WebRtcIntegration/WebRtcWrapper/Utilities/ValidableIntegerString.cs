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

namespace PeerConnectionClient.Utilities
{
    /// <summary>
    /// Class to validate that the string member variable can be converted 
    /// to an integer in range [minValue, maxValue].
    /// </summary>
    public class ValidableIntegerString : ValidableBase<string>
    {
        // Minimum allowed value for the integer
        private readonly int _minValue;

        // Maximum allowed value for the integer
        private readonly int _maxValue;

        /// <summary>
        /// Default constructor to set minimum and maximum integer default values.
        /// </summary>
        public ValidableIntegerString()
        {
            _minValue = 0;
            _maxValue = 100;
        }

        /// <summary>
        /// Constructor
        /// </summary>
        /// <param name="minValue">Minimum allowed value for the integer.</param>
        /// <param name="maxValue">Maximum allowed value for the integer.</param>
        public ValidableIntegerString(int minValue = 0, int maxValue = 100)
        {
            _minValue = minValue;
            _maxValue = maxValue;
        }

        /// <summary>
        /// Constructor.
        /// </summary>
        /// <param name="defaultValue">Default integer value.</param>
        /// <param name="minValue">Minimum allowed value for the integer.</param>
        /// <param name="maxValue">Maximum allowed value for the integer.</param>
        public ValidableIntegerString(int defaultValue, int minValue = 0, int maxValue = 100)
            : this(minValue, maxValue)
        {
            Value = defaultValue.ToString();
        }

        /// <summary>
        /// Constructor
        /// </summary>
        /// <param name="defaultValue">Default integer value.</param>
        public ValidableIntegerString(string defaultValue)
        {
            Value = defaultValue;
        }

        /// <summary>
        /// Validates that the string value can be converted to an integer
        /// and the intereger will be in range [minValue, maxValue].
        /// </summary>
        override protected void Validate()
        {
            try
            {
                var intVal = Convert.ToInt32(Value);
                if (intVal >= _minValue && intVal <= _maxValue)
                {
                    Valid = true;
                }
                else
                {
                    Valid = false;
                }
            }
            catch (Exception)
            {
                Valid = false;
            }
        }
    }
}

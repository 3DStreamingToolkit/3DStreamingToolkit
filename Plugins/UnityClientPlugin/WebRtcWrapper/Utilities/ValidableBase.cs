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

using System.Xml.Serialization;

namespace PeerConnectionClient.Utilities
{
    /// <summary>
    /// A base class for validable values.
    /// </summary>
    /// <typeparam name="T">The type of the value.</typeparam>
    //public abstract class ValidableBase<T> : BindableBase
    public abstract class ValidableBase<T>
    {
        private T _value;

        /// <summary>
        /// The value to validate.
        /// </summary>
        public T Value
        {
            get { return _value; }
            set
            {
                _value = value;
                Validate();                
            }
        }

        [XmlIgnore]
        bool _valid = true;
       
        /// <summary>
        /// Property to indicate if the value is valid.
        /// </summary>
        [XmlIgnore]
        public bool Valid
        {
            get { return _valid; }
            protected set
            {
                _valid = value;
            }
        }

        /// <summary>
        /// Validate that the value meets the requirements for the
        /// specific validable classes.
        /// </summary>
        abstract protected void Validate();
    }
}

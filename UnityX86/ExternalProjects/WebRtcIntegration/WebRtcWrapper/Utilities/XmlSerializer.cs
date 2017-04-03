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

using System.IO;
using System.Text;
using System.Xml;
using System.Xml.Serialization;

namespace PeerConnectionClient.Utilities
{
    internal static class XmlSerializer<T>
    {
        /// <summary>
        /// Serialize to xml.
        /// </summary>
        public static string ToXml(T value)
        {
            XmlSerializer serializer = new XmlSerializer(typeof(T));
            StringBuilder stringBuilder = new StringBuilder();
            XmlWriterSettings settings = new XmlWriterSettings
            {
                Indent = true,
                OmitXmlDeclaration = true,
            };

            using (XmlWriter xmlWriter = XmlWriter.Create(stringBuilder, settings))
            {
                serializer.Serialize(xmlWriter, value);
            }
            return stringBuilder.ToString();
        }

        /// <summary>
        /// Deserialize from xml.
        /// </summary>
        public static T FromXml(string xml)
        {
            XmlSerializer serializer = new XmlSerializer(typeof(T));
            T value;
            using (StringReader stringReader = new StringReader(xml))
            {
                object deserialized = serializer.Deserialize(stringReader);
                value = (T)deserialized;
            }

            return value;
        }
    }
}

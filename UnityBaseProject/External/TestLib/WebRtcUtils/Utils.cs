using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Org.WebRtc;

using Windows.UI.Core;

namespace WebRtcUtils
{
    public static class Utils
    {
        public static string Version()
        {
            return "v1.0-alpha";    
        }

        public static string CpuUsage()
        {
            return Org.WebRtc.WebRTC.CpuUsage.ToString();
        }

        public static string LibTestCall()
        {
            //StringBuilder sb = new StringBuilder();
            //foreach (var vc in Org.WebRtc.WebRTC.GetVideoCodecs())
            //{
            //    sb.AppendLine(vc.Name);
            //}
            //return sb.ToString();

            return Org.WebRtc.RTCSdpType.Answer.ToString();
        }
    }
}

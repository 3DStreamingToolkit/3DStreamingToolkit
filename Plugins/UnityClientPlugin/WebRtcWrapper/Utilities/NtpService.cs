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
using System.Diagnostics;
using Windows.UI.Popups;
using Windows.UI.Core;

using System.Runtime.InteropServices.WindowsRuntime;
using Windows.UI.Xaml;
using Windows.Networking.Sockets;

namespace PeerConnectionClient.Utilities
{
    //public class NtpService : DispatcherBindableBase
    public class NtpService
    {

        public event Action<long> OnNTPTimeAvailable;
        public event Action OnNTPSyncFailed;

        private readonly CoreDispatcher _uiDispatcher;
        private Stopwatch ntpResponseMonitor = new Stopwatch();
        private DispatcherTimer ntpQueryTimer = null;
        private DatagramSocket ntpSocket = null;
        private DispatcherTimer ntpRTTIntervalTimer = null;

        private float averageNtpRTT; //ms
        private int minNtpRTT; //ms
        private int currentNtpQueryCount;
        private int ntpProbeFaileCount;

        private const int MaxNtpProbeFaileCount = 5;
        private const int MaxNtpRTTProbeQuery = 100; // the attempt to get average RTT for NTP query/response

        //public NtpService(CoreDispatcher uiDispatcher) : base(uiDispatcher)
        public NtpService(CoreDispatcher uiDispatcher)
        {
            _uiDispatcher = uiDispatcher;
        }

        /// <summary>
        /// NTP query timeout handler
        /// </summary>
        private void NTPQueryTimeout(object sender, object e)
        {
            if (ntpResponseMonitor.IsRunning)
            {
                ntpResponseMonitor.Stop();
                ntpQueryTimer.Stop();
                ntpProbeFaileCount++;
                Debug.WriteLine(String.Format("NTPSync: timeout ({0}/{1})", ntpProbeFaileCount, MaxNtpProbeFaileCount));
                if (ntpProbeFaileCount >= MaxNtpProbeFaileCount)
                {
                    ReportNtpSyncStatus(false);
                }
                else
                {
                    // Send another NTP query
                    RunOnUiThread(() =>
                    {
                        ntpRTTIntervalTimer.Start();
                    });
                }
            }
        }

        /// <summary>
        /// Report NTP syncronization status
        /// </summary>
        private void ReportNtpSyncStatus(bool status, int rtt = 0)
        {
            MessageDialog dialog;
            if (status)
            {
                dialog = new MessageDialog(String.Format("Synced with ntp server. RTT time {0}ms", rtt));
            }
            else
            {
                if(OnNTPSyncFailed != null)
                    OnNTPSyncFailed.Invoke();
                dialog = new MessageDialog("Failed To sync with ntp server.");
            }

            RunOnUiThread(async () =>
            {
                ntpRTTIntervalTimer.Stop();
                await dialog.ShowAsync();
            });

        }

        public void AbortSync()
        {
            if (ntpRTTIntervalTimer != null)
            {
                ntpRTTIntervalTimer.Stop();
            }

            if (ntpQueryTimer != null)
            {
                ntpQueryTimer.Stop();
            }

            ntpResponseMonitor.Stop();
        }

        /// <summary>
        /// Retrieve the current network time from ntp server  "time.windows.com".
        /// </summary>
        public async void GetNetworkTime(string ntpServer)
        {
            averageNtpRTT = 0; //reset

            currentNtpQueryCount = 0; //reset
            minNtpRTT = -1; //reset

            ntpProbeFaileCount = 0; //reset

            //NTP uses UDP
            ntpSocket = new DatagramSocket();
            ntpSocket.MessageReceived += OnNTPTimeReceived;


            if (ntpQueryTimer == null)
            {
                ntpQueryTimer = new DispatcherTimer();
                ntpQueryTimer.Tick += NTPQueryTimeout;
                ntpQueryTimer.Interval = new TimeSpan(0, 0, 5); //5 seconds
            }

            if (ntpRTTIntervalTimer == null)
            {
                ntpRTTIntervalTimer = new DispatcherTimer();
                ntpRTTIntervalTimer.Tick += SendNTPQuery;
                ntpRTTIntervalTimer.Interval = new TimeSpan(0, 0, 0, 0, 200); //200ms
            }

            ntpQueryTimer.Start();

            try
            {
                //The UDP port number assigned to NTP is 123
                await ntpSocket.ConnectAsync(new Windows.Networking.HostName(ntpServer), "123");
                ntpRTTIntervalTimer.Start();

            }
            catch (Exception e)
            {
                Debug.WriteLine(String.Format("[Error] NTPSync: exception when connect socket: {0}", e.Message));
                ntpResponseMonitor.Stop();
                ReportNtpSyncStatus(false);
            }
        }

        private async void SendNTPQuery(object sender, object e)
        {
            // NTP query pending
            if (ntpResponseMonitor.IsRunning)
                return;

            currentNtpQueryCount++;
            // NTP message size - 16 bytes of the digest (RFC 2030)
            byte[] ntpData = new byte[48];

            //Setting the Leap Indicator, Version Number and Mode values
            ntpData[0] = 0x1B; //LI = 0 (no warning), VN = 3 (IPv4 only), Mode = 3 (Client Mode)

            ntpQueryTimer.Start();

            ntpResponseMonitor.Restart();
            await ntpSocket.OutputStream.WriteAsync(ntpData.AsBuffer());
        }

        /// <summary>
        /// Event hander when receiving response from the ntp server.
        /// </summary>
        /// <param name="socket">The udp socket object which triggered this event </param>
        /// <param name="eventArguments">event information</param>
        private void OnNTPTimeReceived(DatagramSocket socket, DatagramSocketMessageReceivedEventArgs eventArguments)
        {
            int currentRTT = (int)ntpResponseMonitor.ElapsedMilliseconds;

            Debug.WriteLine(String.Format("NTPSync: probe {0} currentRTT={1}", currentNtpQueryCount, currentRTT));

            ntpResponseMonitor.Stop();

            if (currentNtpQueryCount < MaxNtpRTTProbeQuery)
            {
                // we only trace 'min' RTT within the RTT probe attempts
                if (minNtpRTT == -1 || minNtpRTT > currentRTT)
                {

                    minNtpRTT = currentRTT;

                    if (minNtpRTT == 0)
                        minNtpRTT = 1; //in case we got response so  fast, consider it to be 1ms.
                }

                averageNtpRTT = (averageNtpRTT * (currentNtpQueryCount - 1) + currentRTT) / currentNtpQueryCount;

                if (averageNtpRTT < 1)
                {
                    averageNtpRTT = 1;
                }

                RunOnUiThread(() =>
                {
                    ntpQueryTimer.Stop();
                    ntpRTTIntervalTimer.Start();
                });

                return;
            }

            //if currentRTT is good enough, e.g.: closer to minRTT, then, we don't have to continue to query.
            if (currentRTT > (averageNtpRTT + minNtpRTT) / 2)
            {
                RunOnUiThread(() =>
                {
                    ntpQueryTimer.Stop();
                    ntpRTTIntervalTimer.Start();
                });

                return;
            }


            byte[] ntpData = new byte[48];

            eventArguments.GetDataReader().ReadBytes(ntpData);

            //Offset to get to the "Transmit Timestamp" field (time at which the reply 
            //departed the server for the client, in 64-bit timestamp format."
            const byte serverReplyTime = 40;

            //Get the seconds part
            ulong intPart = BitConverter.ToUInt32(ntpData, serverReplyTime);

            //Get the seconds fraction
            ulong fractPart = BitConverter.ToUInt32(ntpData, serverReplyTime + 4);

            //Convert from big-endian to machine endianness
            if (BitConverter.IsLittleEndian)
            {
                intPart = SwapEndianness(intPart);
                fractPart = SwapEndianness(fractPart);
            }

            ulong milliseconds = (intPart * 1000) + ((fractPart * 1000) / 0x100000000L);

            socket.Dispose();
            ReportNtpSyncStatus(true, currentRTT);

            RunOnUiThread(() =>
            {
                if(OnNTPTimeAvailable != null)
                    OnNTPTimeAvailable.Invoke((long)milliseconds + currentRTT / 2);
            });
        }

        private uint SwapEndianness(ulong x)
        {
            return (uint)(((x & 0x000000ff) << 24) +
                          ((x & 0x0000ff00) << 8) +
                          ((x & 0x00ff0000) >> 8) +
                          ((x & 0xff000000) >> 24));
        }

        protected void RunOnUiThread(Action fn)
        {
            var asyncOp = _uiDispatcher.RunAsync(CoreDispatcherPriority.Normal, new DispatchedHandler(fn));
        }

    }
}

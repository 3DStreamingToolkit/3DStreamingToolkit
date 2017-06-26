using System;
using System.Collections;
using System.Collections.Generic;
using System.Collections.Concurrent;
using System.Linq;
using System.Runtime.InteropServices;
using UnityEngine;
using UnityEngine.UI;
using System.Text;

public class ControlScript : MonoBehaviour
{
    public string fileTest = @"bbb_stereo.mp4";
    public string m3u8 = @"https://bitmovin-a.akamaihd.net/content/playhouse-vr/m3u8s/105560.m3u8";
    public string dash = @"http://amssamples.streaming.mediaservices.windows.net/b6822ec8-5c2b-4ae0-a851-fd46a78294e9/ElephantsDream.ism/manifest(format=mpd-time-csf,filtername=FirstFilter)";
    public string hls = @"http://amssamples.streaming.mediaservices.windows.net/55034fb3-11af-43e4-a4de-d1b3ca56c5aa/ElephantsDream_MultiAudio.ism/manifest(format=m3u8-aapl)";
    public string dash4k = @"http://amssamples.streaming.mediaservices.windows.net/634cd01c-6822-4630-8444-8dd6279f94c6/CaminandesLlamaDrama4K.ism/manifest(format=mpd-time-csf)";
    public string hls4k = @"http://amssamples.streaming.mediaservices.windows.net/634cd01c-6822-4630-8444-8dd6279f94c6/CaminandesLlamaDrama4K.ism/manifest(format=m3u8-aapl)";
    public string hlsLive = @"http://b028.wpc.azureedge.net/80B028/Samples/0e8848ca-1db7-41a3-8867-fe911144c045/d34d8807-5597-47a1-8408-52ec5fc99027.ism/manifest(format=m3u8-aapl)";
    public string dashLive = @"http://vm2.dashif.org/livesim/scte35_1/testpic_2s/Manifest.mpd";

    public Texture2D primaryPlaybackTexture;
    public RawImage Canvas;

    internal void Play()
    {
        Plugin.LoadAdaptiveContent(m3u8);
        Plugin.Play();
    }

    private void Awake()
    {
        fileTest = (Application.dataPath + "/StreamingAssets" + "/" + fileTest).Replace("/", "\\");
    }

    private void OnDestroy()
    {
    }

    IEnumerator Start()
    {
        yield return StartCoroutine("CallPluginAtEndOfFrames");
    }

    private void OnEnable()
    {
        Plugin.CreateMediaPlayback();

        GetPlaybackTextureFromPlugin();
    }

    private void OnDisable()
    {
        Plugin.ReleaseMediaPlayback();
    }

    private void Update()
    {
        //Plugin.GetPlaybackState(ref NewPlaybackState);

        //if (OldPlaybackState.type != NewPlaybackState.type)
        //{
        //    OldPlaybackState = NewPlaybackState;

        //    var stateType = (StateType)Enum.ToObject(typeof(StateType), NewPlaybackState.type);

        //    switch (stateType)
        //    {
        //        case StateType.StateType_StateChanged:
        //            PlaybackState playbackState = (PlaybackState)Enum.ToObject(typeof(PlaybackState), NewPlaybackState.state);
        //            Debug.Log("Playback State: " + stateType.ToString() + " - " + playbackState.ToString());
        //            break;
        //        case StateType.StateType_Opened:
        //            MEDIA_DESCRIPTION description = NewPlaybackState.description;
        //            Debug.Log("Media Opened: " + description.ToString());
        //            break;

        //        default:
        //            break;
        //    }
        //}

        if (Input.GetKeyUp(KeyCode.Alpha1))
        {
            StartPlayback(fileTest);
        }
        else if (Input.GetKeyUp(KeyCode.Alpha2))
        {
            StartAdaptivePlayback(m3u8);
        }
        else if (Input.GetKeyUp(KeyCode.Alpha3))
        {
            StartAdaptivePlayback(dash);
        }
        else if (Input.GetKeyUp(KeyCode.Alpha4))
        {
            StartAdaptivePlayback(dash4k);
        }
        else if (Input.GetKeyUp(KeyCode.Alpha5))
        {
            StartAdaptivePlayback(hls);
        }
        else if (Input.GetKeyUp(KeyCode.Alpha6))
        {
            StartAdaptivePlayback(hls4k);
        }
        else if (Input.GetKeyUp(KeyCode.Alpha7))
        {
            StartAdaptivePlayback(hlsLive);
        }
        else if (Input.GetKeyUp(KeyCode.Space))
        {
            Plugin.Stop();
        }
    }

    private void GetPlaybackTextureFromPlugin()
    {
        IntPtr nativeTex = IntPtr.Zero;
        Plugin.GetPrimaryTexture(1920, 1080, out nativeTex);

        primaryPlaybackTexture = Texture2D.CreateExternalTexture(1920, 1080, TextureFormat.BGRA32, false, false, nativeTex);

        // Set texture onto our material
        Canvas.texture = primaryPlaybackTexture;
    }

    private IEnumerator CallPluginAtEndOfFrames()
    {
        while (true)
        {
            // Wait until all frame rendering is done
            yield return new WaitForEndOfFrame();

            // Set time for the plugin
            Plugin.SetTimeFromUnity(Time.timeSinceLevelLoad);

            // Issue a plugin event with arbitrary integer identifier.
            // The plugin can distinguish between different
            // things it needs to do based on this ID.
            // For our simple plugin, it does not matter which ID we pass here.
            GL.IssuePluginEvent(Plugin.GetRenderEventFunc(), 1);
        }
    }

    private void StartPlayback(string file)
    {
        Plugin.LoadContent(file);
        Plugin.Play();
    }

    private void StartAdaptivePlayback(string file)
    {
        Plugin.LoadAdaptiveContent(file);
        Plugin.Play();
    }

    void OnGUI()
    {
        var y = 10;
        var space = 35;

        if (GUI.Button(new Rect(10, y, 100, 30), "Play file"))
        {
            StartPlayback(fileTest);
        }

        y += space;

        if (GUI.Button(new Rect(10, y, 100, 30), "Play .m3u8"))
        {
            StartAdaptivePlayback(m3u8);
        }

        y += space;

        if (GUI.Button(new Rect(10, y, 100, 30), "Play DASH"))
        {
            StartAdaptivePlayback(dash);
        }

        y += space;

        if (GUI.Button(new Rect(10, y, 100, 30), "Play DASH 4K"))
        {
            StartAdaptivePlayback(dash4k);
        }

        y += space;

        if (GUI.Button(new Rect(10, y, 100, 30), "Play HLS"))
        {
            StartAdaptivePlayback(hls);
        }

        y += space;

        if (GUI.Button(new Rect(10, y, 100, 30), "Play HLS 4k"))
        {
            StartAdaptivePlayback(hls4k);
        }

        y += space;

        if (GUI.Button(new Rect(10, y, 100, 30), "Play HLS Live"))
        {
            StartAdaptivePlayback(hlsLive);
        }

        y += space;

        if (GUI.Button(new Rect(10, y, 100, 30), "Play DASH Live"))
        {
            StartAdaptivePlayback(dashLive);
        }

        y += space;

        if (GUI.Button(new Rect(10, y, 100, 30), "Stop"))
        {
            Plugin.Stop();
        }
    }

    public enum PlaybackState
    {
        PlaybackState_None = 0,
        PlaybackState_Opening,
        PlaybackState_Buffering,
        PlaybackState_Playing,
        PlaybackState_Paused,
        PlaybackState_Ended
    };

    public enum StateType
    {
        StateType_None = 0,
        StateType_Opened,
        StateType_StateChanged,
        StateType_Failed,
    };

    [StructLayout(LayoutKind.Sequential)]
    public struct MEDIA_DESCRIPTION
    {
        public UInt32 width;
        public UInt32 height;
        public Int64 duration;
        public byte isSeekable;

        public override string ToString()
        {
            StringBuilder sb = new StringBuilder();
            sb.AppendLine("width: " + width);
            sb.AppendLine("height: " + height);
            sb.AppendLine("duration: " + duration);
            sb.AppendLine("canSeek: " + isSeekable);

            return sb.ToString();
        }
    };

    [StructLayout(LayoutKind.Explicit)]
    public struct PLAYBACK_STATE
    {
        [FieldOffset(0)]
        public UInt16 type;

        [FieldOffset(8)]
        public UInt16 state;

        [FieldOffset(8)]
        public Int64 hresult;

        [FieldOffset(8)]
        public MEDIA_DESCRIPTION description;
    };

    private static class Plugin
    {
        [DllImport("TexturesUWP", CallingConvention = CallingConvention.StdCall, EntryPoint = "CreateMediaPlayback")]
        internal static extern void CreateMediaPlayback();

        [DllImport("TexturesUWP", CallingConvention = CallingConvention.StdCall, EntryPoint = "ReleaseMediaPlayback")]
        internal static extern void ReleaseMediaPlayback();

        [DllImport("MediaPlayback", CallingConvention = CallingConvention.StdCall, EntryPoint = "GetPlaybackState")]
        internal static extern void GetPlaybackState(ref PLAYBACK_STATE state);

        [DllImport("TexturesUWP", CallingConvention = CallingConvention.StdCall, EntryPoint = "GetPrimaryTexture")]
        internal static extern void GetPrimaryTexture(UInt32 width, UInt32 height, out System.IntPtr playbackTexture);

        [DllImport("TexturesUWP", CallingConvention = CallingConvention.StdCall, EntryPoint = "LoadContent")]
        internal static extern void LoadContent([MarshalAs(UnmanagedType.BStr)] string sourceURL);

        [DllImport("TexturesUWP", CallingConvention = CallingConvention.StdCall, EntryPoint = "LoadAdaptiveContent")]
        internal static extern void LoadAdaptiveContent([MarshalAs(UnmanagedType.BStr)] string manifestURL);

        [DllImport("TexturesUWP", CallingConvention = CallingConvention.StdCall, EntryPoint = "Play")]
        internal static extern void Play();

        [DllImport("TexturesUWP", CallingConvention = CallingConvention.StdCall, EntryPoint = "Pause")]
        internal static extern void Pause();

        [DllImport("TexturesUWP", CallingConvention = CallingConvention.StdCall, EntryPoint = "Stop")]
        internal static extern void Stop();

        // Unity plugin
        [DllImport("TexturesUWP", CallingConvention = CallingConvention.StdCall, EntryPoint = "SetTimeFromUnity")]
        internal static extern void SetTimeFromUnity(float t);

        [DllImport("TexturesUWP", CallingConvention = CallingConvention.StdCall, EntryPoint = "GetRenderEventFunc")]
        internal static extern IntPtr GetRenderEventFunc();
    }
}

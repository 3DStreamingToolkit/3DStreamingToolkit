using System.Collections;
using System.Collections.Generic;
using UnityEngine;
using UnityEngine.UI;

#if !UNITY_EDITOR
using WebRtcUtils;
#endif

public class WebRtcHelpers : MonoBehaviour
{
    public Text statusText;
        
    public void VersionCheck()
    {
#if !UNITY_EDITOR
        statusText.text = "UWP 10 - Version: " + Utils.LibTestCall();        
#else
        statusText.text = "NET 3.5 Script";
#endif
    }
}

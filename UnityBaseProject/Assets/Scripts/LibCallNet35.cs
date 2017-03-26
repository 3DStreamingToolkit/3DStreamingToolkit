using System.Collections;
using System.Collections.Generic;
using UnityEngine;
using UnityEngine.UI;

using TestLibNet35;

public class LibCallNet35 : MonoBehaviour
{
    public Text statusText;
        
    public void Net35Call()
    {
#if UNITY_EDITOR
        var call = new TestLibNet35.Net35();
        statusText.text = call.Hello("Confirm NET35");
#else
        statusText.text = "EDITOR ONLY";
#endif
    }
}

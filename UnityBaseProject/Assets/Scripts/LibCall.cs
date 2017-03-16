using System.Collections;
using System.Collections.Generic;
using UnityEngine;
using UnityEngine.UI;

#if UNITY_EDITOR
using NETTestLib;
#else
using UWPTestLib;
#endif 

public class LibCall : MonoBehaviour
{
    public Text statusText;

    public void Call()
    {
        var testClass = new TestClass();
        statusText.text = testClass.Call("Hello!");
    }
}

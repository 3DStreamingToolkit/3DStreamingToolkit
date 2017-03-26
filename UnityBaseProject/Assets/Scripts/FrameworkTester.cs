using System.Collections;
using System.Collections.Generic;
using UnityEngine;
using UnityEngine.UI;

#if UNITY_EDITOR
using Net35;
#else
using Net46;
#endif


public class FrameworkTester : MonoBehaviour
{
    public Text statusText;

    public void Call()
    {
#if UNITY_EDITOR
        statusText.text += TestClass35.Hello("Framework!");
#else
        statusText.text += TestClass46.Hello("Framework!");
#endif
    }
}

using System.Collections;
using System.Collections.Generic;
using UnityEngine;
using UnityEngine.UI;
using TestLib;

public class LibCall : MonoBehaviour
{
    public Text statusText;

    public void Call()
    {
        var testClass = new TestClass1();
        statusText.text = testClass.Echo("Hello!");
    }
}

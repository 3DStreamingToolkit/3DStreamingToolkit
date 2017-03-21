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
        var call = new Net35Lib();
        statusText.text = call.Hello("Confirm NET35");
    }
}

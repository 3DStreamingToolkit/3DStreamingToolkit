using System.Collections;
using System.Collections.Generic;
using UnityEngine;
using UnityEngine.UI;

public class consoleBillboarding : MonoBehaviour {

    public Camera m_Camera;
    public Vector3 consoleOffset = new Vector3(-0.12f, -0.21f, 2.0f);
    public Text textOutput;

    private float _deltaTime;

    void Update() {

        // POSITION AND ROTATE CONSOLE WINDOW TO CONTINUOUSLY STAY 
        // BEFORE TH CAMERA AND ROTATED TO FACE IT
        transform.position = m_Camera.transform.position + (m_Camera.transform.rotation * consoleOffset);
        transform.LookAt(transform.position + m_Camera.transform.rotation * Vector3.forward,
            m_Camera.transform.rotation * Vector3.up);


        // UPDATE THE CONSOLE FEEDBACK WINDOW WITH POSITION, ROTATION, AND FPS
        _deltaTime += (Time.deltaTime - _deltaTime) * .1f;
        var msec = _deltaTime * 1000.0f;
        var fps = 1.0f / _deltaTime;
        var txtFPS = string.Format("{0:0.0} ms ({1:0.} fps)", msec, fps);

        var buildString = "Camera Position: " + m_Camera.transform.localPosition.ToString();
        buildString += "  |  Euler Angles: " + m_Camera.transform.eulerAngles.ToString();
        buildString += "\n" + txtFPS;

        textOutput.text = buildString;

    }
}

using System.Collections;
using System.Collections.Generic;
using UnityEngine;

public class CameraControl : MonoBehaviour
{
    public float MoveRate = 1f;
    public float TurnRate = 90f;

    void Start()
    {

    }

    void Update()
    {
        var pitch = Input.GetAxis("Vertical");
        var yaw = Input.GetAxis("Horizontal");

        var mx = Input.GetAxis("MoveX");
        var my = Input.GetAxis("MoveY");
        var mz = Input.GetAxis("MoveZ");

        var MouseX = Input.GetAxis("Mouse X");
        var MouseY = Input.GetAxis("Mouse Y");

        // Right Mouse Button
        if (Input.GetMouseButton(1))
        {
            transform.Rotate(new Vector3(-MouseY, MouseX, 0f) * Time.deltaTime * TurnRate);
        }

        // Left Mouse Button
        if (Input.GetMouseButton(0))
        {            
            transform.Translate(new Vector3(MouseX, 0f, MouseY) * Time.deltaTime * MoveRate);
        }

        transform.Rotate(new Vector3(pitch, yaw, 0f) * Time.deltaTime * TurnRate);
        transform.Translate(new Vector3(mx, my, mz) * Time.deltaTime * MoveRate);
    }
}

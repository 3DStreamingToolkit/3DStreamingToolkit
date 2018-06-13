using System.Collections;
using System.Collections.Generic;
using UnityEngine;

public class UIControls : MonoBehaviour
{    
    public RectTransform panelMain;

    private IEnumerator panelCoRoutineControl;
    private float height;
    private float xPos;
    private float yPos;
    private bool isPanelOpen;
    private float moveSpeed;

    void Start()
    {
        height = panelMain.rect.height;
        moveSpeed = height * 3f;

        xPos = panelMain.rect.x;
        isPanelOpen = true;
        panelCoRoutineControl = panelCoRoutine(height, 1);
    }

    public void TogglePanel()
    {        
        if (isPanelOpen)
        {
            ClosePanel();
        }
        else
        {
            OpenPanel();
        }
        isPanelOpen = !isPanelOpen;
    }


    public void OpenPanel()
    {
        StopCoroutine(panelCoRoutineControl);
        panelCoRoutineControl = panelCoRoutine(0, 1);
        StartCoroutine(panelCoRoutineControl);
    }

    public void ClosePanel()
    {
        StopCoroutine(panelCoRoutineControl);
        panelCoRoutineControl = panelCoRoutine(-height, -1);
        StartCoroutine(panelCoRoutineControl);
    }

    IEnumerator panelCoRoutine(float yTargetValue, int direction)
    {     
        var targetY = yTargetValue;
        yPos = panelMain.anchoredPosition.y;

        while (
                ((direction > 0) &&  (yPos < targetY)) || 
                ((direction < 0) && (yPos > targetY))         
              )
        {
            yPos += direction * moveSpeed * Time.deltaTime;
            panelMain.anchoredPosition = new Vector3(xPos, yPos);
            yield return null;
        }
        panelMain.anchoredPosition = new Vector3(xPos, yTargetValue);
        yield return null;
    }   
}

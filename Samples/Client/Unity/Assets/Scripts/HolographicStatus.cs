using System.Collections;
using System.Collections.Generic;
using UnityEngine;
using UnityEngine.UI;

public class HolographicStatus : MonoBehaviour
{
	[Tooltip("The left eye status text")]
	public Text LeftStatus;

	[Tooltip("The right eye status text")]
	public Text RightStatus;

	[Tooltip("The time in seconds a status message is shown")]
	public float FlashTimeout = 2f;

	[Tooltip("should the status fade out? otherwise it'll just disappear immediately")]
	public bool FadeOut = true;

	[Tooltip("The time in seconds it takes to fade the object out (if FadeOut is true)")]
	public float FadeTimeout = 2f;

	private Queue<string> statusQueue = new Queue<string>();
	private float currentMessageTime = 0f;

	public void ShowStatus(string status)
	{
		statusQueue.Enqueue(status);

		// if we're off, turn ourselves back on
		if (!this.enabled)
		{
			this.enabled = true;
		}
	}

	private void Start()
	{
		// we want to start ready for messages
		this.currentMessageTime = this.FlashTimeout + (this.FadeOut ? this.FadeTimeout : 0);
	}

	private void Update()
	{

		if (currentMessageTime < FlashTimeout + (FadeOut ? FadeTimeout : 0))
		{
			currentMessageTime += Time.deltaTime;
			return;
		}
		
		if (statusQueue.Count == 0)
		{
			// we have nothing to do until someone queues something
			// so we can turn off
			enabled = false;
			return;
		}
		
		currentMessageTime = 0f;

		var nextMessage = statusQueue.Dequeue();

		LeftStatus.text = nextMessage;
		RightStatus.text = nextMessage;
		
		if (!LeftStatus.enabled)
		{
			LeftStatus.enabled = true;
		}

		if (!RightStatus.enabled)
		{
			RightStatus.enabled = true;
		}

		LeftStatus.CrossFadeAlpha(1, 1, false);
		RightStatus.CrossFadeAlpha(1, 1, false);

		StartCoroutine(FadeOutStatus(FlashTimeout, FadeTimeout));
	}

	private IEnumerator FadeOutStatus(float delayTime, float fadeTime)
	{
		yield return new WaitForSeconds(delayTime);

		LeftStatus.CrossFadeAlpha(0, fadeTime, false);
		RightStatus.CrossFadeAlpha(0, fadeTime, false);
	}
}
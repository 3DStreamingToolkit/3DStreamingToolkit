using UnityEngine;
using UnityEditor;
using UnityEngine.TestTools;
using NUnit.Framework;
using System.Collections;
using UnityEngine.UI;
using System.Linq;
using System;

public class HolographicStatusTests
{
	[UnityTest]
	public IEnumerator HolographicStatusBasicTextAppears()
	{
		var go = new GameObject("HoloStatusTest");
		var holo = go.AddComponent<HolographicStatus>();

		var canvas = new GameObject("HoloStatusCanvas").AddComponent<Canvas>();
		var leftText = new GameObject("Left").AddComponent<Text>();
		var rightText = new GameObject("Right").AddComponent<Text>();

		leftText.transform.SetParent(canvas.transform);
		rightText.transform.SetParent(canvas.transform);

		leftText.enabled = false;
		rightText.enabled = false;
		
		holo.LeftStatus = leftText;
		holo.RightStatus = rightText;

		var expectedText = "test";

		// simulate unity start hook
		holo.Invoke("Start", 0f);

		holo.ShowStatus(expectedText);

		// simulate unity update hook
		holo.Invoke("Update", 0f);

		// skip a frame (to let the text auto update)
		yield return null;

		Assert.AreEqual(expectedText, leftText.text);
		Assert.IsTrue(leftText.enabled);
		Assert.AreEqual(expectedText, rightText.text);
		Assert.IsTrue(rightText.enabled);
	}
}

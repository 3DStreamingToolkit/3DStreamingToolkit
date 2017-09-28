using System.Collections;
using System.Collections.Generic;
using UnityEngine;

using UnityEngine.Windows.Speech;
using System.Linq;

public class VoiceManager : MonoBehaviour
{

    // VOICE COMMANDS WILL BE CONVERTED TO ACTIONABLE EVENTS
    public delegate void OnVoiceCommandEvent(string command);
    public event OnVoiceCommandEvent OnVoiceCommand;

    KeywordRecognizer keywordRecognizer = null;
    Dictionary<string, System.Action> keywords = new Dictionary<string, System.Action>();

    void Start()
    {
        SetupVoiceCommands();
    }

    void SetupVoiceCommands()
    {
        keywords.Add("Boost", () =>
        {
            OnVoiceCommand("boost");

        });

        keywords.Add("Drop", () =>
        {
            OnVoiceCommand("drop");

        });

        keywordRecognizer = new KeywordRecognizer(keywords.Keys.ToArray());
        keywordRecognizer.OnPhraseRecognized += KeywordRecognizer_OnPhraseRecognized;
        keywordRecognizer.Start();
    }

    private void KeywordRecognizer_OnPhraseRecognized(PhraseRecognizedEventArgs args)
    {
        System.Action keywordAction;
        if (keywords.TryGetValue(args.text, out keywordAction))
        {
            keywordAction.Invoke();
        }
    }

}
using UnityEngine;

namespace Microsoft.Toolkit.ThreeD
{
    /// <summary>
    /// Simple behaviour capable of limiting the framerate
    /// </summary>
    class FramerateLimiter : MonoBehaviour
    {
        /// <summary>
        /// The target framerate
        /// </summary>
        public int FPS = 60;

        /// <summary>
        /// Unity engine object Awake() hook
        /// </summary>
        private void Awake()
        {
            if (QualitySettings.vSyncCount > 0)
            {
                // disable vsync, since we want to hit a particular framerate
                QualitySettings.vSyncCount = 0;
            }

            // limit the frame rate
            Application.targetFrameRate = this.FPS;
        }
    }
}

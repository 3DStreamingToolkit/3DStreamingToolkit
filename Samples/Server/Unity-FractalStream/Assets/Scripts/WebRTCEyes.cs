using System.Collections.Generic;
using UnityEngine;

namespace Microsoft.Toolkit.ThreeD
{
    /// <summary>
    /// Represents 1 or 2 eyes used as the viewport for a <see cref="WebRTCServer"/>
    /// </summary>
    public class WebRTCEyes : MonoBehaviour
    {
        /// <summary>
        /// Represents the possible number of eyes
        /// </summary>
        public enum EyeCount
        {
            One,
            Two
        }

        /// <summary>
        /// Total number of eyes
        /// </summary>
        public EyeCount TotalEyes = EyeCount.One;

        /// <summary>
        /// How many eyes we currently render with
        /// </summary>
        public EyeCount VisibleEyes = EyeCount.One;

        /// <summary>
        /// Camera for first (left-most) eye
        /// </summary>
        public Camera EyeOne;

        /// <summary>
        /// Camera for second (from the left) eye
        /// </summary>
        /// <remarks>
        /// In a 2 eye (most common) scenario this is also known as the right eye
        /// </remarks>
        public Camera EyeTwo;

        /// <summary>
        /// The eye count from the last <see cref="SetupActiveEyes"/> call
        /// </summary>
        private EyeCount? lastSetupVisibleEyes = null;

        /// <summary>
        /// List of all camera for all eyes
        /// </summary>
        public IList<Camera> AllEyeCameras
        {
            get
            {
                var res = new List<Camera>();

                if (EyeOne != null)
                {
                    res.Add(EyeOne);
                }

                if (TotalEyes == EyeCount.Two && EyeTwo != null)
                {
                    res.Add(EyeTwo);
                }

                return res;
            }
        }

        /// <summary>
        /// Unity engine object Start() hook
        /// </summary>
        private void Start()
        {
            // do some special setup for stereo, if you aren't stereo, we return
            if (this.TotalEyes != EyeCount.Two)
            {
                return;
            }
            
            // TODO(bengreenier): make this configurable (static value, vs this calculation)
            // calculate and configure our stereo distance based on camera transform distance in the scene
            var stereoDistance = Vector3.Distance(this.EyeOne.transform.position, this.EyeTwo.transform.position);
            
            this.EyeOne.stereoSeparation = stereoDistance;
            this.EyeTwo.stereoSeparation = stereoDistance;

            // configure the eyes at start
            SetupActiveEyes();
        }

        /// <summary>
        /// Unity engine object Uodate() hook
        /// </summary>
        private void Update()
        {
            // if the eye config changes, reconfigure
            if (this.lastSetupVisibleEyes.HasValue &&
                this.lastSetupVisibleEyes.Value != this.VisibleEyes)
            {
                SetupActiveEyes();
            }
        }

        /// <summary>
        /// Setup eye cameras based on number of eyes
        /// </summary>
        private void SetupActiveEyes()
        {
            switch (this.VisibleEyes)
            {
                case EyeCount.One:
                    // none means use the eye like a single camera, which is what we want here
                    this.EyeOne.stereoTargetEye = StereoTargetEyeMask.None;
                    this.EyeTwo.enabled = false;
                    break;
                case EyeCount.Two:
                    this.EyeOne.stereoTargetEye = StereoTargetEyeMask.Left;
                    this.EyeTwo.enabled = true;
                    break;
            }
            
            // update our last setup visible eye value
            this.lastSetupVisibleEyes = this.VisibleEyes;
        }
    }
}

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
        }
    }
}

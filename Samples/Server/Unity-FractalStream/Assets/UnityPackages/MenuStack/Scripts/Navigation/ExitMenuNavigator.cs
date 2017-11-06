using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using UnityEngine;
using UnityEngine.SceneManagement;

namespace MenuStack.Navigation
{
    /// <summary>
    /// Represents a menu navigator that exits the application
    /// </summary>
    public class ExitMenuNavigator : BaseMenuNavigator
    {
        /// <summary>
        /// The name of the scene this navigator navigates to
        /// </summary>
        [Tooltip("The name of the scene this navigator navigates to")]
        public string SceneName;

        /// <summary>
        /// Not implemented
        /// </summary>
        /// <remarks>
        /// Since the <see cref="ExitMenuNavigator"/> overrides navigate to make a raw call
        /// we do not actually need the implemented <see cref="GetNavigationLocation"/> - however, it must have some implementation
        /// because it's an abstract method in <see cref="BaseMenuNavigator"/> (by design, navigators should not use this pattern)
        /// but exit is special :)
        /// </remarks>
        /// <returns></returns>
        protected override Menu GetNavigationLocation()
        {
            throw new NotImplementedException();
        }

        /// <summary>
        /// Triggers navigation
        /// </summary>
        public override void Navigate()
        {
#if UNITY_EDITOR
            Debug.Log("ExitMenuNavigator tried to exit the scene, but since you're running in the editor it paused.");
            UnityEditor.EditorApplication.isPaused = true;
#else
            Application.Quit();
#endif
        }
    }
}

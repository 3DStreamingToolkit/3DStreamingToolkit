using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using UnityEngine;
using UnityEngine.SceneManagement;

namespace MenuStack.Navigation
{
    /// <summary>
    /// Represents a menu navigator that loads a specific scene using the <see cref="SceneManager"/>
    /// </summary>
    public class SceneMenuNavigator : BaseMenuNavigator
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
        /// Since the <see cref="SceneMenuNavigator"/> overrides navigate to make a raw call
        /// we do not actually need the implemented <see cref="GetNavigationLocation"/> - however, it must have some implementation
        /// because it's an abstract method in <see cref="BaseMenuNavigator"/> (by design, navigators should not use this pattern)
        /// but scene is special :)
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
            SceneManager.LoadScene(this.SceneName);
        }
    }
}

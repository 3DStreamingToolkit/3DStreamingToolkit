using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;
using UnityEngine;

namespace MenuStack.Navigation
{
    /// <summary>
    /// Represents a menu navigator that navigates to the first <see cref="Menu"/> in the <see cref="MenuRoot"/> thats name matches some regex
    /// </summary>
    public class RegexMenuNavigator : BaseMenuNavigator
    {
        /// <summary>
        /// The regex string with which we try to match menus
        /// </summary>
        /// <remarks>
        /// We'll start at <see cref="MenuRoot"/> and match the first found
        /// </remarks>
        public string Regex;

        /// <summary>
        /// Provides a location to navigate to, when navigation occurs
        /// </summary>
        /// <returns><see cref="Menu"/> to navigate to</returns>
        protected override Menu GetNavigationLocation()
        {
            return FindFirstRecursively(this.GetComponentInParent<MenuRoot>().transform, new Regex(this.Regex));
        }

        /// <summary>
        /// Internal recursive matcher
        /// </summary>
        /// <param name="root">the root level transform to start matching</param>
        /// <param name="match">the regex to match against</param>
        /// <returns>a matched <see cref="Menu"/>, or <c>null</c></returns>
        public Menu FindFirstRecursively(Transform root, Regex match)
        {
            for (var i = 0; i < root.childCount; i++)
            {
                var child = root.GetChild(i);
                var menu = child.GetDerivedMenuComponent();
                
                if (menu != null && match.IsMatch(menu.name))
                {
                    return menu;
                }

                if (child.childCount > 0)
                {
                    var res = FindFirstRecursively(child, match);

                    if (res != null)
                    {
                        return res;
                    }
                }
            }

            return null;
        }
    }
}

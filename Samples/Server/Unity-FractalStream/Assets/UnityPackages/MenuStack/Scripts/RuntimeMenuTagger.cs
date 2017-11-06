using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using UnityEngine;

namespace MenuStack
{
    /// <summary>
    /// The menu tagger that's used at runtime to find and tag menus in a given hierarchy
    /// </summary>
    public class RuntimeMenuTagger
    {
        /// <summary>
        /// The root of the hierarchy
        /// </summary>
        private Transform root;

        /// <summary>
        /// The prefix with which menus begin
        /// </summary>
        private string menuPrefix;

        /// <summary>
        /// The prefix with which overlays begin
        /// </summary>
        private string overlayPrefix;
        
        /// <summary>
        /// Default ctor
        /// </summary>
        /// <param name="root">root of the heirarchy</param>
        /// <param name="menuPrefix">prefix for menus</param>
        /// <param name="overlayPrefix">prefix for overlays</param>
        public RuntimeMenuTagger(Transform root, string menuPrefix, string overlayPrefix)
        {
            this.root = root;
            this.menuPrefix = menuPrefix;
            this.overlayPrefix = overlayPrefix;
        }
        
        /// <summary>
        /// Tag and get results
        /// </summary>
        /// <returns>tag results</returns>
        public IEnumerable<Menu> Tag()
        {
            return this.TagFromRoot(this.root);
        }

        /// <summary>
        /// Internal tag helper
        /// </summary>
        /// <param name="tagRoot">a root to start tagging from</param>
        /// <returns>tag results</returns>
        private IEnumerable<Menu> TagFromRoot(Transform tagRoot)
        {
            List<Menu> tracked = new List<Menu>();
            for (var i = 0; i < tagRoot.childCount; i++)
            {
                var child = tagRoot.GetChild(i);
                var menu = child.GetComponent<Menu>();

                if (child.name.StartsWith(this.menuPrefix) && menu == null)
                {
                    menu = child.gameObject.AddComponent<Menu>();
                }
                else if (child.name.StartsWith(this.overlayPrefix) && menu == null)
                {
                    menu = child.gameObject.AddComponent<OverlayMenu>();
                }

                if (menu != null)
                {
                    tracked.Add(menu);
                }

                if (child.childCount > 0)
                {
                    tracked.AddRange(TagFromRoot(child));
                }
            }

            return tracked;
        }
    }
}

using System;
using UnityEngine;

namespace Microsoft.Toolkit.ThreeD
{
    /// <summary>
    /// Build task that copies the <see cref="WebRTCServer"/> config files to the output directory if they exist
    /// </summary>
    public class ConfigCopyBuildTask : MonoBehaviour
    {
        /// <summary>
        /// Unity editor post build hook
        /// </summary>
        [UnityEditor.Callbacks.PostProcessBuild]
        static void OnPostprocessBuild(
            UnityEditor.BuildTarget target,
            string pathToBuiltProject)
        {
            string sourcePath = Application.dataPath + "/Plugins/";
            string destPath = pathToBuiltProject.Substring(
                0, pathToBuiltProject.LastIndexOf("/") + 1);

            try
            {
                UnityEditor.FileUtil.CopyFileOrDirectory(
                    sourcePath + "nvEncConfig.json",
                    destPath + "nvEncConfig.json");

                UnityEditor.FileUtil.CopyFileOrDirectory(
                    sourcePath + "webrtcConfig.json",
                    destPath + "webrtcConfig.json");
            }
            catch (Exception ex)
            {
                // Note to developers: If you don't want this behavior for your project, simply delete this script
                Debug.LogWarning("[BuildTask] Unable to copy config files: " + ex.Message);
            }
        }
    }
}
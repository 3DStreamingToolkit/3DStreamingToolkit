using System;
using UnityEngine;

namespace Microsoft.Toolkit.ThreeD.Editor
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
            string dllSourcePath = Application.dataPath + "/Plugins/x64/StreamingServer/";
            string destPath = pathToBuiltProject.Substring(
                0, pathToBuiltProject.LastIndexOf("/") + 1);

            try
            {
                UnityEditor.FileUtil.CopyFileOrDirectory(
                    dllSourcePath + "nvpipe.dll",
                    destPath + "nvpipe.dll");

                UnityEditor.FileUtil.CopyFileOrDirectory(
                    dllSourcePath + "cudart64_91.dll",
                    destPath + "cudart64_91.dll");

                UnityEditor.FileUtil.CopyFileOrDirectory(
                    dllSourcePath + "nvToolsExt64_1.dll",
                    destPath + "nvToolsExt64_1.dll");

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
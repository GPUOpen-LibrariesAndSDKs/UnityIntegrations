//
// Copyright 2017 ADVANCED MICRO DEVICES, INC.  All Rights Reserved.
//
// AMD is granting you permission to use this software and documentation (if
// any) (collectively, the “Materials”) pursuant to the terms and conditions
// of the Software License Agreement included with the Materials.  If you do
// not have a copy of the Software License Agreement, contact your AMD
// representative for a copy.
// You agree that you will not reverse engineer or decompile the Materials,
// in whole or in part, except as allowed by applicable law.
//
// WARRANTY DISCLAIMER: THE SOFTWARE IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND.  AMD DISCLAIMS ALL WARRANTIES, EXPRESS, IMPLIED, OR STATUTORY,
// INCLUDING BUT NOT LIMITED TO THE IMPLIED WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE, TITLE, NON-INFRINGEMENT, THAT THE SOFTWARE
// WILL RUN UNINTERRUPTED OR ERROR-FREE OR WARRANTIES ARISING FROM CUSTOM OF
// TRADE OR COURSE OF USAGE.  THE ENTIRE RISK ASSOCIATED WITH THE USE OF THE
// SOFTWARE IS ASSUMED BY YOU.
// Some jurisdictions do not allow the exclusion of implied warranties, so
// the above exclusion may not apply to You.
//
// LIMITATION OF LIABILITY AND INDEMNIFICATION:  AMD AND ITS LICENSORS WILL
// NOT, UNDER ANY CIRCUMSTANCES BE LIABLE TO YOU FOR ANY PUNITIVE, DIRECT,
// INCIDENTAL, INDIRECT, SPECIAL OR CONSEQUENTIAL DAMAGES ARISING FROM USE OF
// THE SOFTWARE OR THIS AGREEMENT EVEN IF AMD AND ITS LICENSORS HAVE BEEN
// ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.
// In no event shall AMD's total liability to You for all damages, losses,
// and causes of action (whether in contract, tort (including negligence) or
// otherwise) exceed the amount of $100 USD.  You agree to defend, indemnify
// and hold harmless AMD and its licensors, and any of their directors,
// officers, employees, affiliates or agents from and against any and all
// loss, damage, liability and other expenses (including reasonable attorneys'
// fees), resulting from Your use of the Software or violation of the terms and
// conditions of this Agreement.
//
// U.S. GOVERNMENT RESTRICTED RIGHTS: The Materials are provided with "RESTRICTED
// RIGHTS." Use, duplication, or disclosure by the Government is subject to the
// restrictions as set forth in FAR 52.227-14 and DFAR252.227-7013, et seq., or
// its successor.  Use of the Materials by the Government constitutes
// acknowledgement of AMD's proprietary rights in them.
//
// EXPORT RESTRICTIONS: The Materials may be subject to export restrictions as
// stated in the Software License Agreement.
//

// -------------------------------------------------------------------------------
// Amf 360 Player Instructions:
//
// This script makes use of the AmfUnityStitchMediaPlugin. The AmfUnityStitchMediaPlugin must be
// included for this script to work.
//
// Add two cameras to the scene, one for each eye. Around each camera create a
// sphere that faces inwards, this can be done either via a script (such as
// http://wiki.unity3d.com/index.php/ReverseNormals) or through building a new
// sphere model. Create two materials, one for each eye. Apply each material to
// the appropriate sphere. Create two new GameObject layers, one for each eye.
// Set each eye sphere's object layer to only the layer for that eye. Set the
// culling mask for each camera to the matching object layer. After this set up
// there should be 2 cameras and 2 spheres, each with a matching culling mask and
// object layer. Each sphere should also have it's own material matching material.
// 
// Turn off the main directional light and set the ambient light color in the
// Environment Lighting to white (1,1,1). Without this change the video will
// be lit by the main directional light and have corresponding shadows, when it
// is turned off only ambient light will affect it.
//
// Create an empty GameObject and attach this script to it. In the inspector,
// assign the materials used for the left and right eye spheres. Select the
// appropriate Stereoscopic Format that matches the way the video is
// packaged, e.g. Left eye on the top, right eye on the bottom (Top Bottom).
//
// If the video is not aligned to the center of your screen initially, consider
// changing the sphere's starting rotation. If the video is not correctly
// aligned on the sphere, such as being upside down or backwards, consider
// changing the offset and scaling values of the materials set in the
// 'CreateTextureAndPassToPlugin' method to match the sphere construction.
// -------------------------------------------------------------------------------

using UnityEngine;
using System;
using System.Collections;
using System.Runtime.InteropServices;
using System.IO;
using System.Collections.Generic;

public class AmfUnityStitchMediaPlugin : MonoBehaviour
{
    #region Stitching DLL imports
    // Extern functions
    [DllImport("AmfUnityStitchMediaPlugin")]
    private static extern void SetTextureFromUnity(int id, IntPtr texture, int w, int h);
    [DllImport("AmfUnityStitchMediaPlugin")]
    private static extern IntPtr GetRenderEventFunc();

    // Execute operations are:
    // init, file://, terminate, play, pause, step, stop
    [DllImport("AmfUnityStitchMediaPlugin")]
    private static extern void PipelineCreate(int id);
    [DllImport("AmfUnityStitchMediaPlugin")]
    private static extern void PipelineDestroy(int id);
    [DllImport("AmfUnityStitchMediaPlugin")]
    private static extern int PipelineExecute(int id, [MarshalAs(UnmanagedType.LPWStr)] string cmd);

    // Query types are:
    // fps, progressSize, progressPosition, texWidth, texHeight, sampleRate,
    // channels, hasAudio, initialized, running
    [DllImport("AmfUnityStitchMediaPlugin")]
    private static extern float PipelineQuery(int id, [MarshalAs(UnmanagedType.LPWStr)] string type);

    [DllImport("AmfUnityStitchMediaPlugin")]
    private static extern int PipelineFillAudio(int id, ref float outData, int bufferSize);
    #endregion

    #region Properties visible in Unity editor
    // PTS Gui File (required)
    public string PtsFile = "RicohThetaS.pts";
    // Media file - unstitched video file
    public string MediaFile = "Assets/Movies/TestUnstitched.mp4";

    public bool mute;
    [Tooltip("Material to contain the stitched video")]
    public Material SkySphereMaterial;

    [Tooltip("Sets target material's Albedo map to the stitched video texture")]
    public bool OverrideMaterialAlbedo = false;
    [Tooltip("Sets target material's Emissive map to the stitched video texture.\nEmissive must be enabled and intensity set in the editor.")]
    public bool OverrideMaterialEmissive = true;

    //Camera live capture Mode - matches enum in the stitch DLL
    public enum CAMLIVE_MODE_ENUM
    {
        CAMLIVE_MODE_ZCAM_1080P24 = 0,  //1920x1080, 24FPS
        CAMLIVE_MODE_ZCAM_1080P30,      //1920x1080, 30FPS
        CAMLIVE_MODE_ZCAM_1080P60,      //1920x1080, 60FPS
        CAMLIVE_MODE_ZCAM_2K7P24,       //2704x1520, 24FPS
        CAMLIVE_MODE_ZCAM_2K7P30,       //2704x1520, 24FPS
        CAMLIVE_MODE_ZCAM_2K7P60,       //2704x1520, 24FPS
        CAMLIVE_MODE_ZCAM_2544P24,      //3392x2544, 24FPS
        CAMLIVE_MODE_ZCAM_2544P30,      //3392x2544, 24FPS
        CAMLIVE_MODE_ZCAM_2544P60,      //3392x2544, 24FPS
        CAMLIVE_MODE_THETAS,            //Ricoh TheataS
        CAMLIVE_MODE_THETAV,            //Ricoh TheataV
        CAMLIVE_MODE_INVALID = -1,
    };
    public CAMLIVE_MODE_ENUM CameraMode;

    [Tooltip("Override the MediaFile setting and use the camera as the video source")]
    public bool ForceCamera = false;

    #endregion

    // Private variables
    enum AVState { kNone, kInit, kPlay, kPause };
    private AVState _avState = AVState.kNone;
    private int _uniqueID = 0;
    private bool _playAudio = false;
    private int _samplerate = 48000;
    private int _channels = 2;
    private AudioSource _audioSource;
    private AudioClip _audioClip;
    private Texture2D _videoTexture;

    /// <summary>
    /// Validates plugin settings and initializes the AMF pipeline. 
    /// </summary>
    /// <returns></returns>
    IEnumerator Start()
    {
        ProcessCommandLineArguments();
        if (!ValidateState())
        {
            Debug.LogError("Component state is invalid.");
            enabled = false;
            yield break;
        }

        _uniqueID = GetInstanceID();
        try
        {
            PipelineCreate(_uniqueID);

            var dllPath = Application.dataPath.Replace('\\', '/') + "/Plugins/";
            PipelineExecute(_uniqueID, "dllPath://" + dllPath);

            var assetPath = Application.streamingAssetsPath.Replace('\\', '/') + "/";

            // plugin assumes DLLs are in dataPath/plugins, and files are in streamingAssets/
            PipelineExecute(_uniqueID, "pts://" + assetPath + PtsFile);

            if (!string.IsNullOrEmpty(MediaFile))
            {
                if (File.Exists(MediaFile))
                {
                    PipelineExecute(_uniqueID, "media://" + MediaFile);

                }
                else
                {
                    PipelineExecute(_uniqueID, "media://" + assetPath + MediaFile);
                }
            }

            PipelineExecute(_uniqueID, "cameramode://" + ((int)CameraMode).ToString());
            PipelineExecute(_uniqueID, "forcecamera://" + (ForceCamera ? "1" : "0"));


            PipelineExecute(_uniqueID, "init");
            if (PipelineQuery(_uniqueID, "initialized") > 0)
            {
                PipelineExecute(_uniqueID, "play");
                CreateAndSetVideoTexture();

                _playAudio = (PipelineQuery(_uniqueID, "hasAudio") > 0.0f) && (!mute);
                if (_playAudio)
                {
                    CreateAudio();
                }
                _avState = AVState.kPause;
            }
            else
            {
                print("Failed to initialize pipeline\n");
                enabled = false;
                yield break;
            }

        } 
        catch(Exception ex)
        {
            print("Failed to initialize pipeline. An exception occurred. Disabling plugin.\n");
            Debug.LogException(ex);
            enabled = false;
            yield break;

        }

        yield return StartCoroutine("PluginUpdateTexture");
    }


    /// <summary>
    /// Validates some plugin settings. Call this before initializing the pipeline. 
    /// </summary>
    /// <returns></returns>
    bool ValidateState()
    {

        if (string.IsNullOrEmpty(PtsFile))
        {
            Debug.LogError("Pts GUI file is empty, but is required for stitching\n");
            return false;
        }

        var ptsPath = Path.Combine(Application.streamingAssetsPath, PtsFile);

        if (!File.Exists(ptsPath))
        {
            Debug.LogError("PTS Gui file '" + ptsPath + "' does not exist\n");
            return false;
        }

        var mediaPath = Path.Combine(Application.streamingAssetsPath, MediaFile);

        if (!string.IsNullOrEmpty(MediaFile) && !(File.Exists(MediaFile) ||File.Exists(mediaPath)))
        {
            print("MediaFile is set to '" + mediaPath + "', but the file does not exist\n");
            return false;
        }

        return true;
    }

    /// <summary>
    /// Shuts down the AMF pipeline when the player stops. 
    /// </summary>
    void OnDestroy()
    {
        PipelineExecute(_uniqueID, "terminate");
        PipelineDestroy(_uniqueID);
    }


    /// <summary>
    /// Create a video texture and pass a reference to the stitching DLL. 
    /// 
    /// </summary>
    private void CreateAndSetVideoTexture()
    {
        // Get dimensions of the stitched video from the plugin
        int w = (int)PipelineQuery(_uniqueID, "texWidth");
        int h = (int)PipelineQuery(_uniqueID, "texHeight");

        Debug.Assert(w > 0, "Invalid width " + w.ToString() + " from plugin\n");
        Debug.Assert(h > 0, "Invalid height " + w.ToString() + " from plugin\n");

        // TODO: query stitch DLL for texture format?
        _videoTexture = new Texture2D(w, h, TextureFormat.RGBA32, false);
        _videoTexture.Apply();

        // Pass texture pointer to the plugin
        SetTextureFromUnity(_uniqueID, _videoTexture.GetNativeTexturePtr(), _videoTexture.width, _videoTexture.height);

        if (OverrideMaterialAlbedo)
        {
            SkySphereMaterial.mainTexture = _videoTexture;
        }

        if (OverrideMaterialEmissive)
        {
            SkySphereMaterial.SetTexture("_EmissionMap", _videoTexture);
        }
    }

    /// <summary>
    /// Tells the stitcher to update our texture. This should be called every frame. 
    /// </summary>
    /// <returns></returns>
    private IEnumerator PluginUpdateTexture()
    {
        while (true)
        {
            // Wait until all frame rendering is done
            yield return new WaitForEndOfFrame();

            // Issue a plugin event with arbitrary integer identifier.
            // The plugin can distinguish between different
            // things it needs to do based on this ID.
            // For our simple plugin, it does not matter which ID we pass here.
            GL.IssuePluginEvent(GetRenderEventFunc(), _uniqueID);
        }
    }

    private void CreateAudio()
    {
        return;
        // Query the audio setup of the video
        _samplerate = (int)PipelineQuery(_uniqueID, "sampleRate");
        _channels = (int)PipelineQuery(_uniqueID, "channels");

        // Create streaming audio clip
        _audioClip =
             AudioClip.Create("StreamingAudio", _samplerate * _channels * 10, _channels, _samplerate, true);
        // Create audio source
        _audioSource = gameObject.AddComponent<AudioSource>();
        _audioSource.volume = 1.0f;
        _audioSource.spatialBlend = 1.0f;
        _audioSource.ignoreListenerVolume = false;
        _audioSource.loop = true;
        _audioSource.clip = _audioClip;
        _audioSource.maxDistance = 10;
        _audioSource.Play();
    }

    void OnAudioFilterRead(float[] data, int channels)
    {
        PipelineFillAudio(_uniqueID, ref data[0], data.Length);
    }

    void ProcessCommandLineArguments()
    {
        var args = Environment.GetCommandLineArgs();

        for (int i = 0; i < args.Length; ++i)
        {
            var key = args[i].ToLower();

            switch (key)
            {
                case "-pts":
                    {
                        if (i < args.Length - 1)
                        {
                            PtsFile = args[i + 1];
                            print("PtsGuiFile override from command line, '" + PtsFile + "'\n");
                            ++i;
                        }
                        else
                        {

                            Debug.LogError("Command line parameter '-pts' found, but no filename specified.");
                        }

                    }
                    break;
                case "-media":
                    {
                        if (i < args.Length - 1)
                        {
                            MediaFile = args[i + 1];
                            print("MediaFile override from command line, '" + PtsFile + "'\n");
                            ++i;
                        }
                        else
                        {
                            Debug.LogError("Command line parameter '-media' found, but no filename specified.");
                        }

                    }
                    break;
                case "-forcecamera":
                    {
                        ForceCamera = true;
                    }
                    break;
                default:
                    break;
            }
        }

        if (ForceCamera)
        {
            MediaFile = string.Empty;
        }

    }

    public void RestartPlugin()
    {

    }

}

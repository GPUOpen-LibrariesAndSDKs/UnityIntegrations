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

using UnityEngine;
using System;
using System.Collections;
using System.Runtime.InteropServices;


public class AmfUnityPlugin : MonoBehaviour
{
	// Extern functions
	[DllImport("AmfUnityPlugin")]
	private static extern void SetTextureFromUnity(int id, IntPtr texture, int w, int h);
	[DllImport("AmfUnityPlugin")]
	private static extern IntPtr GetRenderEventFunc();

	// Execute operations are:
	// init, file://, terminate, play, pause, step, stop
	[DllImport("AmfUnityPlugin")]
	private static extern void PipelineCreate(int id);
	[DllImport("AmfUnityPlugin")]
	private static extern void PipelineDestroy(int id);
	[DllImport("AmfUnityPlugin")]
	private static extern int PipelineExecute(int id, [MarshalAs(UnmanagedType.LPWStr)] string cmd);

    // Query types are:
    // fps, progressSize, progressPosition, texWidth, texHeight, sampleRate,
    // channels, hasAudio, initialized, running
    [DllImport("AmfUnityPlugin")]
	private static extern float PipelineQuery(int id, [MarshalAs(UnmanagedType.LPWStr)] string type);

	[DllImport("AmfUnityPlugin")]
	private static extern int PipelineFillAudio(int id, ref float outData, int bufferSize);

    // Must be called before pipeline is intialized
    [DllImport("AmfUnityPlugin")]
    private static extern int PipelineMuteAudio(int id, bool isMuted);

    // Public variables 
    public string File;
	public bool mute;

	// Private variables
	enum AVState { kNone, kInit, kPlay, kPause };
	private AVState avState = AVState.kNone;
	private int uniqueID = 0;
	private bool playAudio = false;
	private int samplerate = 48000;
	private int channels = 2;
	private AudioSource audioSource;
	private AudioClip audioClip;

	IEnumerator Start()
	{
		// 
		CheckPlatform();
		//
		PrintHelp();
		//
		uniqueID = GetUniqueID();
		//
		PipelineCreate(uniqueID);
        //
		PipelineExecute(uniqueID, "file://" + File);
        PipelineMuteAudio(uniqueID, mute);
		PipelineExecute(uniqueID, "init");
        //
		if (PipelineQuery(uniqueID, "initialized")>0)
		{
			PipelineExecute(uniqueID, "play");
			// The following create methods query the pipeline
			// information so these calls come after init above
			CreateTextureAndPassToPlugin();
			// Create audio objects if needed
			playAudio = (PipelineQuery(uniqueID, "hasAudio") > 0.0f);
			if (playAudio)
			{
				CreateAudio();
			}
			//
			avState = AVState.kPlay;
		}
		else
		{
			print("Failed to initialize pipeline\n");
		}

		yield return StartCoroutine("CallPluginAtEndOfFrames");
	}

	void OnDestroy()
	{
		PipelineExecute(uniqueID, "terminate");
		PipelineDestroy(uniqueID);
	}

	void Update()
	{
		if (Input.GetKeyDown(KeyCode.Space))
		{
			if (AVState.kPlay == avState)
			{
				// Call ensures that pause is not reapplied
				PipelineExecute(uniqueID, "pause");
				//
				if (playAudio)
				{
					audioSource.Stop();
				}
				avState = AVState.kPause;
			}
			else if (AVState.kPause == avState)
			{
				// Call ensures that pause is not reapplied
				PipelineExecute(uniqueID, "play");
				//
				if (playAudio)
				{
					audioSource.Play();
				}
				avState = AVState.kPlay;
			}
		}
		else if (Input.GetKeyDown(KeyCode.RightArrow))
		{
			PipelineExecute(uniqueID, "step");
			//
			if (playAudio)
			{
				audioSource.Stop();
			}
			avState = AVState.kPause;
		}
	}

	private void CreateTextureAndPassToPlugin()
	{
		// Find out the size of the video
		int w = (int) PipelineQuery(uniqueID, "texWidth");
		int h = (int) PipelineQuery(uniqueID, "texHeight");

		// Create a texture
		Texture2D tex = new Texture2D(w, h, TextureFormat.BGRA32, false);
		// Set point filtering just so we can see the pixels clearly
		tex.filterMode = FilterMode.Point;
		// Call Apply() so it's actually uploaded to the GPU
		tex.Apply();

		// Set texture onto our material
		GetComponent<Renderer>().material.mainTexture = tex;

		// Pass texture pointer to the plugin
		SetTextureFromUnity(uniqueID, tex.GetNativeTexturePtr(), tex.width, tex.height);
	}

	private IEnumerator CallPluginAtEndOfFrames()
	{
		while (true)
		{
			// Wait until all frame rendering is done
			yield return new WaitForEndOfFrame();

			// Issue a plugin event with arbitrary integer identifier.
			// The plugin can distinguish between different
			// things it needs to do based on this ID.
			// For our simple plugin, it does not matter which ID we pass here.
			GL.IssuePluginEvent(GetRenderEventFunc(), uniqueID);
		}
	}

	private void CreateAudio()
	{
		// Query the audio setup of the video
		samplerate = (int) PipelineQuery(uniqueID, "sampleRate");
		channels = (int) PipelineQuery(uniqueID, "channels");

		// Create streaming audio clip
		audioClip =
			 AudioClip.Create("StreamingAudio", samplerate * channels * 10, channels, samplerate, true);
		// Create audio source
		audioSource = gameObject.AddComponent<AudioSource>();
		audioSource.volume = 1.0f;
		audioSource.spatialBlend = 1.0f;
		audioSource.ignoreListenerVolume = false;
		audioSource.loop = true;
		audioSource.clip = audioClip;
		audioSource.maxDistance = 10;
		audioSource.Play();
	}

	void OnAudioFilterRead(float[] data, int channels)
	{
		PipelineFillAudio(uniqueID, ref data[0], data.Length);
	}

	void HandlePause(bool pauseStatus)
	{
		if (AVState.kNone == avState)
		{
			return;
		}
		//
		if (pauseStatus)
		{
			avState = AVState.kPause;
		}
		else
		{
			avState = AVState.kPlay;
		}
		//
		if (AVState.kPause == avState)
		{
			PipelineExecute(uniqueID, "pause");
			//
			if (playAudio)
			{
				audioSource.Stop();
			}
		}
		else if (AVState.kPlay == avState)
		{
			PipelineExecute(uniqueID, "play");
			//
			if (playAudio)
			{
				audioSource.Play();
			} 
		}
	}

	void OnApplicationFocus(bool hasFocus)
	{
		HandlePause(!hasFocus);
	}

	void OnApplicationPause(bool pauseStatus)
	{
		HandlePause(pauseStatus);
	}

	int GetUniqueID()
	{
		// Instance ID of an object is guaranteed to be unique
		return GetInstanceID();
	}

	void PrintHelp()
	{
		print("Space bar toggles play/pause. Right arrow steps.\n");
	}

	void CheckPlatform()
	{
		if ((Application.platform != RuntimePlatform.WindowsEditor) &&
			(Application.platform != RuntimePlatform.WindowsPlayer) )
		{
			print("AmfUnityPlugin only works on Windows OS\n");
		}
	}

	void Test()
	{
		// Simple test of pipeline parsing
		int id = uniqueID + 1;
		PipelineCreate(id);
		PipelineExecute(id, "file://Assets/inception.mp4");
		PipelineExecute(id, "init");
		PipelineExecute(id, "play");
		PipelineExecute(id, "pause");
		PipelineExecute(id, "step");
		PipelineExecute(id, "stop");
		PipelineExecute(id, "terminate");
		PipelineExecute(id, "notacmd");

		float result = 0.0f;
		result = PipelineQuery(id, "fps");
		print(result);
		result = PipelineQuery(id, "progressSize");
		print(result);
		result = PipelineQuery(id, "progressPosition");
		print(result);
		result = PipelineQuery(id, "notaquery");

		PipelineDestroy(id);
	}
}

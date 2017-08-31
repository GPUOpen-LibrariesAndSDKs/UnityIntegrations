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

#include "Pipeline/UnityAVPipeline.h"

#include "Unity/IUnityGraphics.h"
#include "Unity/IUnityGraphicsD3D11.h"

// --------------------------------------------------------------------------
// Statics
static UnityGfxRenderer g_DeviceType = kUnityGfxRendererNull;

static IUnityInterfaces* g_UnityInterfaces = NULL;
static IUnityGraphics* g_Graphics = NULL;

static bool g_D3D11Renderer = false;
static ID3D11Device* g_Device = NULL;

// --------------------------------------------------------------------------
// SetTextureFromUnity passes the unity texture into the pipeline
extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API SetTextureFromUnity(int id, void* textureHandle, int w, int h)
{
	// A script calls this at initialization time; just remember the texture pointer here.
	// Will update texture pixels each frame from the plugin rendering event (texture update
	// needs to happen on the rendering thread).
	UnityAVPipeline* avpipe = UnityAVPipeline::GetPipeline(id);
	if (avpipe)
	{
		avpipe->SetTexture(textureHandle);
		avpipe->SetDim(w, h);
	}
}

// --------------------------------------------------------------------------
// Handle device events for D3D11
void ProcessDeviceEvent(UnityGfxDeviceEventType type, IUnityInterfaces* interfaces)
{
	if (!g_D3D11Renderer)
	{
		return;
	}

	switch (type)
	{
		case kUnityGfxDeviceEventInitialize:
		{
			IUnityGraphicsD3D11* d3d = interfaces->Get<IUnityGraphicsD3D11>();
			g_Device = d3d->GetDevice();
			UnityAVPipeline::SetUnityDevice(g_Device);
			break;
		}
		case kUnityGfxDeviceEventShutdown:
			break;
	}
}

// --------------------------------------------------------------------------
// Handle the graphics events
static void UNITY_INTERFACE_API OnGraphicsDeviceEvent(UnityGfxDeviceEventType eventType)
{
	// Create graphics API implementation upon initialization
	if (eventType == kUnityGfxDeviceEventInitialize)
	{
		g_DeviceType = g_Graphics->GetRenderer();
		g_D3D11Renderer = kUnityGfxRendererD3D11 == g_DeviceType;
	}

	// Process the graphic events
	ProcessDeviceEvent(eventType, g_UnityInterfaces);

	// Cleanup graphics API implementation upon shutdown
	if (eventType == kUnityGfxDeviceEventShutdown)
	{
		g_DeviceType = kUnityGfxRendererNull;
	}
}

// --------------------------------------------------------------------------
// The render event handler
static void UNITY_INTERFACE_API OnRenderEvent(int eventID)
{
	UnityAVPipeline* avpipe = UnityAVPipeline::GetPipeline(eventID);
	if (avpipe)
	{
		if (avpipe->initialized())
		{
			avpipe->CopyResource();
		}
	}
}

// --------------------------------------------------------------------------
// Return the render event function to Unity.
extern "C" UnityRenderingEvent UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API GetRenderEventFunc()
{
	return OnRenderEvent;
}

// --------------------------------------------------------------------------
// Execute commands on the pipeline
extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API PipelineCreate(int id)
{
	UnityAVPipeline::CreatePipeline(id);
	if (NULL == UnityAVPipeline::GetUnityDevice())
	{
		UnityAVPipeline::SetUnityDevice(g_Device);
	}
}

// --------------------------------------------------------------------------
// Execute commands on the pipeline
extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API PipelineDestroy(int id)
{
	UnityAVPipeline::DestroyPipeline(id);
}

// --------------------------------------------------------------------------
// Execute commands on the pipeline
extern "C" int UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API PipelineExecute(int id, const wchar_t* cmd)
{
	int result = -1;
	UnityAVPipeline* avpipe = UnityAVPipeline::GetPipeline(id);
	if (avpipe)
	{
		avpipe->PipelineExecute(cmd);
		result = 0;
	}
	return result;
}

// --------------------------------------------------------------------------
// Query state from the pipeline
extern "C" float UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API PipelineQuery(int id, const wchar_t* type)
{
	UnityAVPipeline* avpipe = UnityAVPipeline::GetPipeline(id);
	if (avpipe)
	{
		return avpipe->PipelineQuery(type);
	}
	return 0.0f;
}

// --------------------------------------------------------------------------
// Execute commands on the pipeline
extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API PipelineSetAmbiMode(int id, bool isAmbisonic)
{
	UnityAVPipeline* avpipe = UnityAVPipeline::GetPipeline(id);
	if (avpipe)
	{
		avpipe->SetAmbisonic(isAmbisonic);
	}
}

// --------------------------------------------------------------------------
// Mute audio on pipeline
extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API PipelineMuteAudio(int id, bool isMuted)
{
	UnityAVPipeline* avpipe = UnityAVPipeline::GetPipeline(id);
	if (avpipe)
	{
		avpipe->MutePipeline(isMuted);
	}
}

// --------------------------------------------------------------------------
// Execute commands on the pipeline
extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API PipelineSetAmbisonicAngles(int id, float theta, float phi, float rho)
{
	UnityAVPipeline* avpipe = UnityAVPipeline::GetPipeline(id);
	if (avpipe)
	{
		if (avpipe->PipelineQuery(L"isAmbisonic"))
		{
			avpipe->SetAmbisonicAngles(theta, phi, rho);
		}
	}
}

// --------------------------------------------------------------------------
// Fill in the audio data for unity
extern "C" UNITY_INTERFACE_EXPORT int UNITY_INTERFACE_API PipelineFillAudio(int id, float audioOut[], int bufferLength, int pipelineID)
{
	int result = -1;
	UnityAVPipeline* avpipe = UnityAVPipeline::GetPipeline(id);
	if (avpipe)
	{
		avpipe->PipelineFillAudio(audioOut, bufferLength);
		result = 0;
	}
	return result;
}

//-------------------------------------------------------------------------------------------------
// Main handler which is called when the plugin loads.
//
extern "C" void	UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API
UnityPluginLoad(IUnityInterfaces* unityInterfaces)
{
	g_UnityInterfaces = unityInterfaces;
	g_Graphics = g_UnityInterfaces->Get<IUnityGraphics>();
	g_Graphics->RegisterDeviceEventCallback(OnGraphicsDeviceEvent);

	// Run OnGraphicsDeviceEvent(initialize) manually on plugin load
	OnGraphicsDeviceEvent(kUnityGfxDeviceEventInitialize);
}

//-------------------------------------------------------------------------------------------------
// Unload and terminate handle
//
extern "C" void	UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API
UnityPluginUnload()
{
	g_Graphics->UnregisterDeviceEventCallback(OnGraphicsDeviceEvent);
}

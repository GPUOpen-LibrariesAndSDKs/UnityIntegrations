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

#include "UnityAVPipeline.h"
#include <assert.h>
#include <map>
#include <fstream>

// --------------------------------------------------------------------------
static std::map<int, std::unique_ptr<UnityAVPipeline>> gPipelineMap;

CComQIPtr<ID3D11DeviceContext>	UnityAVPipeline::m_ucontext;
CComQIPtr<ID3D11Device>			UnityAVPipeline::m_udevice;

// --------------------------------------------------------------------------
static bool FileExists(const std::wstring& filePath)
{
	std::ifstream ifile(filePath);
	return ifile.good();
}

// --------------------------------------------------------------------------
UnityAVPipeline* UnityAVPipeline::GetPipeline(int id)
{
	std::map<int, std::unique_ptr<UnityAVPipeline>>::iterator it =
		gPipelineMap.find(id);
	if (it == gPipelineMap.end())
	{
		return NULL;
	}
	//
	return it->second.get();
}

// --------------------------------------------------------------------------
void UnityAVPipeline::CreatePipeline(int id)
{
	if (0 == gPipelineMap.count(id))
	{
		gPipelineMap[id] = std::make_unique<UnityAVPipeline>();
	}
}

// --------------------------------------------------------------------------
void UnityAVPipeline::DestroyPipeline(int id)
{
	if (gPipelineMap.count(id) > 0 )
	{
		gPipelineMap[id].release();
		gPipelineMap.erase(id);
		return;
	}
	DBSTREAMOUT("Pipeline " << id << " not destroyed\n");
}

// --------------------------------------------------------------------------
void UnityAVPipeline::PipelineExecute(const wchar_t* cmd)
{
	if (!cmd)
	{
		return;
	}
	//
	char* str_mod = "";
	if (wcsncmp(cmd, L"file://", 7) == 0)
	{
		SetFilename(cmd);
	}
	else if (wcscmp(cmd, L"init") == 0)
	{
		if (m_fileName.empty())
		{
			DBSTREAMOUT("Filename not set on the pipeline\n");
			return;
		}
		//
		if (!FileExists(m_fileName))
		{
			DBSTREAMOUT("File " << m_fileName << "does not exists\n");
			return;
		}
		//
		SetAMFDevice();
		//
		if (NULL == m_device)
		{
			DBSTREAMOUT("Device not set on the pipeline\n");
			return;
		}
		//
		AMF_RESULT res = AMF_OK;
		res = g_AMFFactory.Init();
		if (AMF_OK != res)
		{
			DBSTREAMOUT("Failed to initialize AMF factory\n");
			g_AMFFactory.Terminate();
			return;
		}
		//
		g_AMFFactory.GetDebug()->AssertsEnable(true);
		//
		m_pipeline = std::make_unique<PlaybackPipeline>();
		m_pipeline->SetParam(PlaybackPipeline::PARAM_NAME_INPUT, m_fileName.c_str());
		m_pipeline->SetParam(PlaybackPipeline::PARAM_NAME_PRESENTER, amf::AMF_MEMORY_DX11);
		//
		UpdateDLLSearchPaths();
		//
		res = m_pipeline->Init(m_device);
		if (AMF_OK != res)
		{
			DBSTREAMOUT("Failed to initialize AMF pipeline\n");
			PipelineExecute(L"terminate");
			return;
		}
		//
		m_initializedPipeline = true;
	}
	else if (wcscmp(cmd, L"terminate") == 0)
	{
		m_pipeline.release();
		g_AMFFactory.Terminate();
		m_texture.Release();
		m_initializedPipeline = false;
	}
	else if (wcscmp(cmd, L"play") == 0)
	{
		if (initialized() && paused())
		{
			m_pipeline->Play();
			//
			m_isPaused = false;
		}
	}
	else if (wcscmp(cmd, L"pause") == 0)
	{
		if (initialized() && (!paused()))
		{
			m_pipeline->Pause();
			//
			m_isPaused = true;
		}
	}
	else if (wcscmp(cmd, L"step") == 0)
	{
		if (initialized())
		{
			m_pipeline->Step();
			//
			m_isPaused = true;
		}
	}
	else if (wcscmp(cmd, L"stop") == 0)
	{
		if (initialized())
		{
			m_pipeline->Stop();
		}
	}
	else
	{
		str_mod = "unknown ";
	}
	DBSTREAMOUT("PipelineExecute: " << str_mod << cmd << std::endl);
}

// --------------------------------------------------------------------------
float UnityAVPipeline::PipelineQuery(const wchar_t* type)
{
	float result = 0.0f;
	if (!type || !initialized())
	{
		return result;
	}
	//
	char* str_mod = "";
	if (wcscmp(type, L"fps") == 0)
	{	// Video
		result = (float)m_pipeline->GetFPS();
	}
	else if (wcscmp(type, L"progressSize") == 0)
	{	// Video
		result = (float)m_pipeline->GetProgressSize();
	}
	else if (wcscmp(type, L"progressPosition") == 0)
	{	// Video
		result = (float)m_pipeline->GetProgressPosition();
	}
	else if (wcscmp(type, L"texWidth") == 0)
	{	// Video
		result = (float)m_pipeline->GetTextureWidth();
	}
	else if (wcscmp(type, L"texHeight") == 0)
	{	// Video
		result = (float)m_pipeline->GetTextureHeight();
	}
	else if(wcscmp(type, L"sampleRate") == 0)
	{	// Audio
		result = (float)m_pipeline->GetSampleRate();
	}
	else if (wcscmp(type, L"channels") == 0)
	{	// Audio
		result = (float)m_pipeline->GetChannels();
	}
	else if (wcscmp(type, L"hasAudio") == 0)
	{	// Audio
		result = (m_pipeline->HasAudio()) ? 1.0f : 0.0f;
	}
	else if (wcscmp(type, L"initialized") == 0)
	{	// Both
		result = (float)initialized();
	}
	else if (wcscmp(type, L"running") == 0)
	{	// Both
		result = (float)pipelineRunning();
	}
	else
	{
		str_mod = "unknown ";
	}
	DBSTREAMOUT("PipelineQuery: " << str_mod << type << " " << result << std::endl);

	return result;
}

// --------------------------------------------------------------------------
void UnityAVPipeline::PipelineFillAudio(float audioOut[], int bufferLength)
{
	if (initialized() && pipelineRunning())
	{
		m_pipeline->GetAudioData(&audioOut[0], bufferLength);
	}
}

// --------------------------------------------------------------------------
void UnityAVPipeline::UpdateDLLSearchPaths()
{
	wchar_t npath[MAX_PATH];
	GetCurrentDirectoryW(MAX_PATH, npath);
	std::wstring path = (const wchar_t*)&npath[0];
	path += L"/Assets";
	if (!SetDllDirectory(path.c_str()))
	{
		DBSTREAMOUT("Failed to update the DLL search paths\n");
		return;
	}
}

// --------------------------------------------------------------------------
void UnityAVPipeline::SetFilename(const wchar_t* urlFileName)
{
	if (initialized() && pipelineRunning())
	{
		DBSTREAMOUT("Cannot change file when pipeline is running\n");
		return;
	}
	//
	std::wstring fn = urlFileName;
	if (0 == fn.find(L"file://"))
	{
		m_fileName = fn.substr(7, std::wstring::npos);
	}
	else
	{
		m_fileName = urlFileName;
	}
}

// --------------------------------------------------------------------------
const std::wstring& UnityAVPipeline::Filename() const
{
	return m_fileName;
}

// --------------------------------------------------------------------------
bool UnityAVPipeline::pipelineRunning()
{
	return (m_pipeline->GetState() == PipelineStateRunning);
}

// --------------------------------------------------------------------------
bool UnityAVPipeline::initialized() const
{
	return m_initializedPipeline;
}

// --------------------------------------------------------------------------
bool UnityAVPipeline::paused() const
{
	return m_isPaused;
}

// --------------------------------------------------------------------------
UnityAVPipeline::UnityAVPipeline()
	: m_initializedPipeline(false)
	, m_width(0)
	, m_height(0)
	, m_isPaused(true)
{
}

// --------------------------------------------------------------------------
UnityAVPipeline::~UnityAVPipeline()
{
	if (initialized())
	{
		PipelineExecute(L"terminate");
	}
}

// --------------------------------------------------------------------------
void UnityAVPipeline::SetDim(unsigned width, unsigned height)
{
	m_width = width;
	m_height = height;
}

// --------------------------------------------------------------------------
void UnityAVPipeline::SetTexture(void *texture)
{
	m_texture = (ID3D11Texture2D*) texture;
}

// --------------------------------------------------------------------------
void UnityAVPipeline::SetUnityDevice(void *device)
{
	// Check device
	if (!device)
	{
		return;
	}

	//
	m_udevice = (ID3D11Device*)device;
	m_udevice->GetImmediateContext(&m_ucontext);
}

// --------------------------------------------------------------------------
void* UnityAVPipeline::GetUnityDevice()
{
	return m_udevice;
}

// --------------------------------------------------------------------------
void UnityAVPipeline::SetAMFDevice()
{
	// Check device
	if (!m_udevice)
	{
		return;
	}

	// Create AMF device from Unity device
	CComQIPtr<IDXGIDevice> dxgiDevice;
	m_udevice->QueryInterface(__uuidof(IDXGIDevice), (void **)& dxgiDevice);
	if (!dxgiDevice)
	{
		return;
	}
	//
	CComQIPtr<IDXGIAdapter> dxgiAdapter;
	dxgiDevice->GetParent(__uuidof(IDXGIAdapter), (void **)& dxgiAdapter);
	if (!dxgiAdapter)
	{
		return;
	}

	//
	// Create the AMF ID3D11Device from the Unity device
#if defined(_DEBUG) 
	UINT createDeviceFlags = D3D11_CREATE_DEVICE_DEBUG;
#else 
	UINT createDeviceFlags = 0;
#endif
	createDeviceFlags |= m_udevice->GetCreationFlags();
	// Turn off D3D11_CREATE_DEVICE_SINGLETHREADED in case it is on
	createDeviceFlags = createDeviceFlags & ~createDeviceFlags;

	D3D_FEATURE_LEVEL featureLevels[] =
	{
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0
	};
	D3D_FEATURE_LEVEL featureLevel;

	// If you set the pAdapter parameter to a non-NULL value, you must also set the 
	// DriverType parameter to the D3D_DRIVER_TYPE_UNKNOWN value (msdn).
	D3D_DRIVER_TYPE eDriverType = D3D_DRIVER_TYPE_UNKNOWN;
	HRESULT hr = D3D11CreateDevice(dxgiAdapter, eDriverType, NULL, createDeviceFlags, featureLevels, _countof(featureLevels),
		D3D11_SDK_VERSION, &m_device, &featureLevel, &m_context);
	if (FAILED(hr))
	{
		hr = D3D11CreateDevice(dxgiAdapter, eDriverType, NULL, createDeviceFlags, featureLevels + 1, _countof(featureLevels) - 1,
			D3D11_SDK_VERSION, &m_device, &featureLevel, &m_context);
		if (FAILED(hr))
		{
			return;
		}
	}
}

// --------------------------------------------------------------------------
void UnityAVPipeline::CopyResource()
{
	if (!m_texture)
	{
		return;
	}

	// We need to lock the presenter while we are copying the resource
	m_pipeline->Lock();
	//
	amf::AMFSurfacePtr pSurf = m_pipeline->GetSurface();
	if (pSurf)
	{
		amf::AMFPlanePtr pPlane = pSurf->GetPlane(amf::AMF_PLANE_PACKED);
		if (pPlane)
		{
			CComQIPtr<ID3D11Texture2D> surfTex = (ID3D11Texture2D*)pPlane->GetNative();
			CComQIPtr<IDXGIResource> pDxgiRes((ID3D11Texture2D*)surfTex);
			if (pDxgiRes)
			{
				HANDLE hShared = 0;
				pDxgiRes->GetSharedHandle(&hShared);
				if (hShared)
				{
					CComQIPtr<ID3D11Resource> resource11;
					m_udevice->OpenSharedResource(hShared, __uuidof(ID3D11Resource), (void**)&resource11);
					if (resource11)
					{
						m_ucontext->CopyResource(m_texture, resource11);
					}
				}
			}
		}
	}
	//
	m_pipeline->Unlock();
}

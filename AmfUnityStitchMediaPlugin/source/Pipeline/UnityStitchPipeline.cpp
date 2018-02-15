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

#include "UnityStitchPipeline.h"
#include <assert.h>
#include <map>
#include <fstream>

#include "public/include/components/ZCamLiveStream.h"
#include "VideoPresenterTexture.h"
#include "public/samples/CPPSamples/common/StitchPipelineBase.h"

// --------------------------------------------------------------------------
static std::map<int, std::unique_ptr<UnityStitchPipeline>> g_pipelineMap;

CComQIPtr<ID3D11DeviceContext>	UnityStitchPipeline::m_unityContext;
CComQIPtr<ID3D11Device>			UnityStitchPipeline::m_unityDevice;


// --------------------------------------------------------------------------
static bool FileExists(const std::wstring& filePath)
{
	std::ifstream ifile(filePath);
	return ifile.good();
}

// --------------------------------------------------------------------------
UnityStitchPipeline* UnityStitchPipeline::GetPipeline(int id)
{
	auto it = g_pipelineMap.find(id);
	if (it == g_pipelineMap.end())
	{
		return NULL;
	}
	//
	return it->second.get();
}

// --------------------------------------------------------------------------
void UnityStitchPipeline::CreatePipeline(int id)
{
	if (0 == g_pipelineMap.count(id))
	{
		g_pipelineMap[id] = std::make_unique<UnityStitchPipeline>();
	}
}

// --------------------------------------------------------------------------
void UnityStitchPipeline::DestroyPipeline(int id)
{
	if (g_pipelineMap.count(id) > 0)
	{
		g_pipelineMap[id].release();
		g_pipelineMap.erase(id);
		return;
	}
	DBSTREAMOUT("Pipeline " << id << " not destroyed\n");
}

bool isCommand(const std::wstring& s, const std::wstring& prefix, std::wstring& parameter)
{
	if (0 == wcsncmp(s.data(), prefix.data(), prefix.length()))
	{
		parameter = s.substr(prefix.length());
		return true;
	}
	return false;
}

// --------------------------------------------------------------------------
void UnityStitchPipeline::PipelineExecute(const wchar_t* cmd)
{
	if (!cmd)
	{
		return;
	}
	std::wstring parameter;

	if (isCommand(cmd, L"media://", parameter))
	{
		if (!FileExists(parameter))
		{
			DBSTREAMOUT(L"Could not find media file '" + parameter);
			return;
		}

		m_mediaFile = parameter;
	}
	else if (isCommand(cmd, L"pts://", parameter))
	{
		if (!FileExists(parameter))
		{
			DBSTREAMOUT(L"Could not find PTS file '" << parameter << L"'");
		}
		else
		{
			m_ptGuiFilepath = parameter;
			DBSTREAMOUT("Using PTS file '" << m_ptGuiFilepath << "'\n");
		}
	}
	else if (isCommand(cmd, L"dllPath://", parameter))
	{
		m_dllFolder = parameter;
		DBSTREAMOUT(L"Setting DLL folder to '" << m_dllFolder << "'\n");
		SetDllDirectory(m_dllFolder.c_str());
	}
	else if (wcscmp(cmd, L"init")==0)
	{
		AMF_RESULT res = InitPipeline();
		if (AMF_OK != res)
		{
			DBSTREAMOUT("Failed to initialize pipeline, res = " << res << '\n');
			return;
		}
	}
	else if (wcscmp(cmd, L"terminate") == 0)
	{
		g_AMFFactory.Terminate();
		m_texture.Release();
		m_initializedPipeline = false;
	}
	else if (wcscmp(cmd, L"play") == 0)
	{
		auto fps = GetFPS();
		DBSTREAMOUT("Fps: " << fps << '\n');
		Play();
	}
	else if (wcscmp(cmd, L"pause") == 0)
	{
		Pause();
	}
	else if (wcscmp(cmd, L"step") == 0)
	{
		Step();
	}
	else if (wcscmp(cmd, L"stop") == 0)
	{
		Stop();
	}
	else
	{
		DBSTREAMOUT("PipelineExecute: " << L"UNKNOWN: " << cmd << std::endl);
	}
}

// --------------------------------------------------------------------------
float UnityStitchPipeline::PipelineQuery(const wchar_t* type)
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
		result = (float)GetFPS();
	}
	else if (wcscmp(type, L"progressSize") == 0)
	{	// Video
		result = (float)GetProgressSize();
	}
	else if (wcscmp(type, L"progressPosition") == 0)
	{	// Video
		result = (float)GetProgressPosition();
	}
	else if (wcscmp(type, L"texWidth") == 0)
	{	// Video
		result = (float)m_width;
	}
	else if (wcscmp(type, L"texHeight") == 0)
	{	// Video
		result = (float)m_height;
	}
	else if (wcscmp(type, L"sampleRate") == 0)
	{	// Audio
		result = 0;// (float)GetSampleRate();
	}
	else if (wcscmp(type, L"channels") == 0)
	{	// Audio
		result = 0;// (float)GetChannels();
	}
	else if (wcscmp(type, L"hasAudio") == 0)
	{	// Audio
		result = 1.f; // m_pipeline->HasAudio()) ? 1.0f : 0.0f;
	}
	else if (wcscmp(type, L"isAmbisonic") == 0)
	{	// Audio
		result = false; // m_pipeline->isAmbisonic() ? 1.0f : 0.0f;
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
bool UnityStitchPipeline::pipelineRunning()
{
	return GetState() == PipelineStateRunning;
}

// --------------------------------------------------------------------------
bool UnityStitchPipeline::initialized() const
{
	return m_initializedPipeline;
}

// --------------------------------------------------------------------------
UnityStitchPipeline::UnityStitchPipeline()
	: m_initializedPipeline(false)
	, m_width(0)
	, m_height(0)
	, m_sharedHandle(NULL)
{
	SetParam(PARAM_NAME_PRESENTER, amf::AMF_MEMORY_DX11);
}

// --------------------------------------------------------------------------
UnityStitchPipeline::~UnityStitchPipeline()
{
	if (initialized())
	{
		PipelineExecute(L"terminate");
	}
}

// --------------------------------------------------------------------------
void UnityStitchPipeline::SetDim(unsigned width, unsigned height)
{
	m_width = width;
	m_height = height;
}

// --------------------------------------------------------------------------
void UnityStitchPipeline::SetTexture(void *texture)
{
	m_texture = (ID3D11Texture2D*)texture;
}

// --------------------------------------------------------------------------
void UnityStitchPipeline::SetUnityDevice(void *device)
{
	// Check device
	if (!device)
	{
		return;
	}

	//
	m_unityDevice = (ID3D11Device*)device;
	m_unityDevice->GetImmediateContext(&m_unityContext);
}

// --------------------------------------------------------------------------
void* UnityStitchPipeline::GetUnityDevice()
{
	return m_unityDevice;
}

// --------------------------------------------------------------------------
void UnityStitchPipeline::MutePipeline(bool isMuted)
{
	//m_pipeline->Mute(isMuted);
}


void UnityStitchPipeline::Lock()
{
	VideoPresenterTexturePtr ptr = std::dynamic_pointer_cast<VideoPresenterTexture>(m_pVideoPresenter);
	if (ptr)
	{
		ptr->Lock();
	}
}

void UnityStitchPipeline::Unlock()
{
	VideoPresenterTexturePtr ptr = std::dynamic_pointer_cast<VideoPresenterTexture>(m_pVideoPresenter);
	if (ptr)
	{
		ptr->Unlock();
	}
}

amf::AMFSurface* UnityStitchPipeline::GetSurface()
{
	VideoPresenterTexturePtr ptr = std::dynamic_pointer_cast<VideoPresenterTexture>(m_pVideoPresenter);
	if (ptr)
	{
		return ptr->GetSurface();
	}
	return NULL;
}

// --------------------------------------------------------------------------
void UnityStitchPipeline::CopyResource()
{
	if (!m_texture)
	{
		return;
	}

	// We need to lock the presenter while we are copying the resource
	Lock();
	//
	amf::AMFSurfacePtr pSurf = GetSurface();
	if (pSurf)
	{
		amf::AMFPlanePtr pPlane = pSurf->GetPlane(amf::AMF_PLANE_PACKED);
		if (pPlane)
		{
			CComQIPtr<ID3D11Texture2D> surfTex = (ID3D11Texture2D*)pPlane->GetNative();
			CComQIPtr<IDXGIResource> pDxgiRes((ID3D11Texture2D*)surfTex);

			if (pDxgiRes)
			{
				pDxgiRes->GetSharedHandle(&m_sharedHandle);
				if (m_sharedHandle)
				{
					CComQIPtr<ID3D11Resource> resource11;
					m_unityDevice->OpenSharedResource(m_sharedHandle, __uuidof(ID3D11Resource), (void**)&resource11);
					if (resource11)
					{
						m_unityContext->CopyResource(m_texture, resource11);
					}
				}
			}
		}
	}
	//
	Unlock();
}



inline std::wstring toWS(const std::string& ss)
{
	std::wstring ws(ss.length(), L' ');
	std::copy(ss.begin(), ss.end(), ws.begin());
	return ws;
}


AMF_RESULT UnityStitchPipeline::InitStitch(
	void* mediaOutput,
	const std::vector<std::wstring>& streamFilePaths,
	const std::wstring& ptGuiFilePath
)
{
	// Check device
	if (!m_unityDevice)
	{
		DBSTREAMOUT("Unity DX11 Device not set\n");
		return AMF_FAIL;
	}

	// Create AMF device from Unity device
	CComQIPtr<IDXGIDevice> dxgiDevice;
	m_unityDevice->QueryInterface(__uuidof(IDXGIDevice), (void **)& dxgiDevice);
	if (!dxgiDevice)
	{
		DBSTREAMOUT("DXGI Device instance not valid.\n")
			return AMF_NOT_INITIALIZED;
	}
	//
	CComQIPtr<IDXGIAdapter> dxgiAdapter;
	dxgiDevice->GetParent(__uuidof(IDXGIAdapter), (void **)& dxgiAdapter);
	if (!dxgiAdapter)
	{
		DBSTREAMOUT("DXGI Adapter instance not valid.\n")

			return AMF_NOT_INITIALIZED;
	}

	//
	// Create the AMF ID3D11Device from the Unity device
	UINT createDeviceFlags = m_unityDevice->GetCreationFlags();
	// Turn off D3D11_CREATE_DEVICE_SINGLETHREADED in case it is on
	createDeviceFlags = createDeviceFlags & ~D3D11_CREATE_DEVICE_SINGLETHREADED;// createDeviceFlags;

#if defined(_DEBUG) 
	createDeviceFlags = D3D11_CREATE_DEVICE_DEBUG;
#endif

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
			DBSTREAMOUT("Could not create D3D11 device\n");
			return AMF_NOT_INITIALIZED;
		}
	}

	const bool forceCamera = false;

	if (forceCamera || m_mediaFile.size() == 0)
	{
		SetParam(PARAM_NAME_ZCAMLIVE_MODE, CAMLIVE_MODE_THETAS);
	}
	else
	{
		DBSTREAMOUT("Using specified stitchable media file '" << m_mediaFile << "'\n");
	}

	SetParamAsString(PARAM_NAME_PTS_FILE, ptGuiFilePath);

	AMF_RESULT result = InitMedia(streamFilePaths, true);
	CHECK_AMF_ERROR_RETURN(result, L"InitMedia() failed!\n");

	result = InitCamera(ptGuiFilePath, true);
	CHECK_AMF_ERROR_RETURN(result, L"InitCamera() failed!\n");


	result = StitchPipelineBase::Init();
	CHECK_AMF_ERROR_RETURN(result, L"StitchPipelineBase::Init() failed!\n");


	return result;
}


AMF_RESULT  UnityStitchPipeline::InitContext(amf::AMF_MEMORY_TYPE type)
{
	if (amf::AMF_MEMORY_DX11 != type)
	{
		DBSTREAMOUT("Context memory type not supported\n");
		return AMF_NOT_SUPPORTED;
	}

	g_AMFFactory.GetFactory()->CreateContext(&m_pContext);
	return m_pContext->InitDX11(m_device.p);
}

AMF_RESULT  UnityStitchPipeline::InitStitcher(amf::AMF_SURFACE_FORMAT &stitchInputFmt)
{
	stitchInputFmt = amf::AMF_SURFACE_NV12;
	//---------------------------------------------------------------------------------------------
	amf_int32 streamCount = (amf_int32)m_Cameras.size();
	//	GetParam(StitchPipelineBase::PARAM_NAME_STREAM_COUNT, streamCount);
	amf_int32 compositedWidth = 1920;
	amf_int32 compositedHeight = 1080;


	GetParam(StitchPipelineBase::PARAM_NAME_COMPOSITED_WIDTH, compositedWidth);
	GetParam(StitchPipelineBase::PARAM_NAME_COMPOSITED_HEIGHT, compositedHeight);

	SetDim(compositedWidth, compositedHeight);


	amf_int64 modeZCam = CAMLIVE_MODE_INVALID;
	GetParam(PARAM_NAME_ZCAMLIVE_MODE, modeZCam);
	bool isZCamLive = modeZCam != CAMLIVE_MODE_INVALID;
	if (isZCamLive && (CAMLIVE_MODE_THETAS != modeZCam) && (CAMLIVE_MODE_THETAV != modeZCam))
	{
		//streamCount = m_pZCamDemuxer->GetOutputCount();
	}

	//---------------------------------------------------------------------------------------------
	// Init Video Stitch
	amf::AMFComponentPtr pStitch;
	std::wstring stitchDll = m_dllFolder;
	stitchDll.append(L"amf-stitch-64.dll");
	AMF_RESULT res = g_AMFFactory.LoadExternalComponent(m_pContext, stitchDll.c_str(), "AMFCreateComponentInt", NULL, &pStitch);
	CHECK_AMF_ERROR_RETURN(res, L"AMFCreateComponent failed");
	m_pStitch = amf::AMFComponentExPtr(pStitch);

	m_pStitch->SetProperty(AMF_VIDEO_STITCH_MEMORY_TYPE, m_pVideoPresenter->GetMemoryType());
	m_pStitch->SetProperty(AMF_VIDEO_STITCH_INPUTCOUNT, amf_int64(streamCount));
	m_pStitch->SetProperty(AMF_VIDEO_STITCH_OUTPUT_SIZE, AMFConstructSize(compositedWidth, compositedHeight));

	m_pStitch->SetProperty(AMF_VIDEO_STITCH_OUTPUT_FORMAT, amf::AMF_SURFACE_RGBA);
	m_pStitch->SetProperty(AMF_VIDEO_STITCH_OUTPUT_MODE, AMF_VIDEO_STITCH_OUTPUT_MODE_EQUIRECTANGULAR);

	return UpdateStitchMode();
}

AMF_RESULT  UnityStitchPipeline::CreateVideoPresenter(amf::AMF_MEMORY_TYPE type, amf_int32 compositedWidth, amf_int32 compositedHeight)
{
	if (amf::AMF_MEMORY_DX11 != type)
		return AMF_NOT_SUPPORTED;

	/*
		if (!m_mediaOutput)
			return AMF_NOT_INITIALIZED;
			*/
	m_pVideoPresenter = std::make_shared<VideoPresenterTexture>();
	m_bVideoPresenterDirectConnect = true;
	/*
	m_bVideoPresenterDirectConnect = true;

	MediaOutput->Tracks.SetVideoTrack(
		LOCTEXT("AMF Video Track Name", "Video Track 0"),
		0,
		FIntPoint(CompositedWidth, CompositedHeight),
		0
	);
	*/

	return AMF_OK;
}

AMF_RESULT UnityStitchPipeline::UpdateStitchMode()
{
	if (m_pStitch != nullptr)// && MediaOutput != nullptr)
	{


		amf::AMF_SURFACE_FORMAT surfaceFormat = amf::AMF_SURFACE_RGBA;
		//switch (MediaOutput->GetSinkFormat())
		//{
		//case EMediaTextureSinkFormat::CharBGRA:
		//	//MM for compatibility with AMF - temporarely
		//	surfaceFormat = amf::AMF_SURFACE_RGBA;
		//	break;
		//case EMediaTextureSinkFormat::FloatRGBA:
		//	surfaceFormat = amf::AMF_SURFACE_RGBA_F16;
		//	break;
		//}
		m_pStitch->SetProperty(AMF_VIDEO_STITCH_OUTPUT_FORMAT, surfaceFormat);

#if defined(USE_CUBEMAP_MEDIATEXTURE)
		IMediaTextureSink* Sink = MediaOutput->GetVideoSink();
		if (Sink != nullptr && Sink->IsCubemap())
		{
			m_pStitch->SetProperty(AMF_VIDEO_STITCH_OUTPUT_MODE, AMF_VIDEO_STITCH_OUTPUT_MODE_CUBEMAP);
			return AMF_OK;
		}
#endif
		m_pStitch->SetProperty(AMF_VIDEO_STITCH_OUTPUT_MODE, AMF_VIDEO_STITCH_OUTPUT_MODE_EQUIRECTANGULAR);

	}
	return AMF_OK;
}

AMF_RESULT UnityStitchPipeline::InitPipeline()
{
	if (!FileExists(m_ptGuiFilepath))
	{
		DBSTREAMOUT("No valid ptGui file. Aborting.");
		return AMF_FILE_NOT_OPEN;
	}

	AMF_RESULT res = g_AMFFactory.Init();
	if (AMF_OK != res)
	{
		DBSTREAMOUT("Failed to initialize AMF factory\n");
		g_AMFFactory.Terminate();
		return res;
	}

	g_AMFFactory.GetDebug()->AssertsEnable(true);

	m_width = 1920;
	m_height = 1080;
	std::vector<std::wstring> filePaths;
	if (m_mediaFile.length() > 0)
	{
		if (FileExists(m_mediaFile))
			filePaths.push_back(m_mediaFile);
		else
			DBSTREAMOUT("Specified media file << '" << m_mediaFile << "' does not exist.\n");
	}

	res = InitStitch(nullptr, filePaths, m_ptGuiFilepath);

	m_initializedPipeline = AMF_OK == res;

	if (!m_initializedPipeline)
	{
		DBSTREAMOUT("Failed to initialize pipeline\n");
		return res;
	}

	return res;
}


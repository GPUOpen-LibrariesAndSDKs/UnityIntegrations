// 
// Notice Regarding Standards.  AMD does not provide a license or sublicense to
// any Intellectual Property Rights relating to any standards, including but not
// limited to any audio and/or video codec technologies such as MPEG-2, MPEG-4;
// AVC/H.264; HEVC/H.265; AAC decode/FFMPEG; AAC encode/FFMPEG; VC-1; and MP3
// (collectively, the "Media Technologies"). For clarity, you will pay any
// royalties due for such third party technologies, which may include the Media
// Technologies that are owed as a result of AMD providing the Software to you.
// 
// MIT license 
// 
// Copyright (c) 2017 Advanced Micro Devices, Inc. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//

#if 0

#include "AmfStitchMediaPipeline.h"
#include "AmfStitchVideoPresenter.h"
//#include "AmfStitchMediaOutput.h"
//#include "VideoStitch.h"
//#include "Engine/Texture.h"

//#include "AllowWindowsPlatformTypes.h"
#include <D3D11.h>
//#include "HideWindowsPlatformTypes.h"

#define LOCTEXT_NAMESPACE "FAmfStitchMediaPipeline"

FAmfStitchMediaPipeline::FAmfStitchMediaPipeline()
	: MediaOutput(nullptr)
{
	SetParam(PARAM_NAME_PRESENTER, amf::AMF_MEMORY_DX11);
}

AMF_RESULT
FAmfStitchMediaPipeline::InitStitch(
	FAmfStitchMediaOutput* InMediaOutput,
	const std::vector<std::wstring>& StreamFilePaths,
	const std::wstring& PtguiFilePath
)
{
	MediaOutput = InMediaOutput;

	ID3D11Device* EngineDevice = static_cast<ID3D11Device*>(GDynamicRHI->RHIGetNativeDevice());

	TRefCountPtr<IDXGIDevice> DxgiDevice;
	EngineDevice->QueryInterface(IID_PPV_ARGS(DxgiDevice.GetInitReference()));
	if (!DxgiDevice)
	{
		return AMF_NOT_INITIALIZED;
	}

	TRefCountPtr<IDXGIAdapter> DxgiAdapter;
	DxgiDevice->GetParent(IID_PPV_ARGS(DxgiAdapter.GetInitReference()));
	if (!DxgiAdapter)
	{
		return AMF_NOT_INITIALIZED;
	}
	constexpr D3D_FEATURE_LEVEL FeatureLevels[] =
	{
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0
	};

	D3D_FEATURE_LEVEL ActualFeatureLevel = static_cast<D3D_FEATURE_LEVEL>(0);
	constexpr D3D_DRIVER_TYPE DriverType = D3D_DRIVER_TYPE_UNKNOWN;
	const UINT DeviceFlags = EngineDevice->GetCreationFlags();


	if(AmfDevice.IsValid() == false)
	{ 


		HRESULT Result = D3D11CreateDevice(
			DxgiAdapter,
			DriverType,
			NULL,
			DeviceFlags,
			FeatureLevels,
			_countof(FeatureLevels),
			D3D11_SDK_VERSION,
			AmfDevice.GetInitReference(),
			&ActualFeatureLevel,
			nullptr // immediate context
		);

		if (FAILED(Result))
		{
			Result = D3D11CreateDevice(
				DxgiAdapter,
				DriverType,
				NULL,
				DeviceFlags,
				&FeatureLevels[1],
				1,
				D3D11_SDK_VERSION,
				AmfDevice.GetInitReference(),
				&ActualFeatureLevel,
				nullptr // immediate context
			);

			if (FAILED(Result))
			{
				return AMF_NOT_INITIALIZED;
			}
		}
	}

	const FString DllPath(FPaths::Combine(
		*FPaths::EngineDir(),
#if PLATFORM_64BITS
		TEXT("Binaries/ThirdParty/AMD/amf/Win64")
#else
		TEXT("Binaries/ThirdParty/AMD/amf/Win32")
#endif
	));
	SetDllDirectory(*DllPath);

	SetParam(AMF_VIDEO_STITCH_INPUTCOUNT, StreamFilePaths.size());

	AMF_RESULT AmfResult = InitMedia(StreamFilePaths, true);
	CHECK_AMF_ERROR_RETURN(AmfResult, L"InitMedia() failed!\n");

	AmfResult = InitCamera(PtguiFilePath, true);
	CHECK_AMF_ERROR_RETURN(AmfResult, L"InitCamera() failed!\n");

	AmfResult = StitchPipelineBase::Init();
	CHECK_AMF_ERROR_RETURN(AmfResult, L"StitchPipelineBase::Init() failed!\n");

	return AmfResult;
}

AMF_RESULT
FAmfStitchMediaPipeline::InitContext(amf::AMF_MEMORY_TYPE Type)
{
	if (Type != amf::AMF_MEMORY_DX11)
	{
		return AMF_NOT_SUPPORTED;
	}

	g_AMFFactory.GetFactory()->CreateContext(&m_pContext);
	return m_pContext->InitDX11(AmfDevice.GetReference());
}

AMF_RESULT
FAmfStitchMediaPipeline::InitStitcher(amf::AMF_SURFACE_FORMAT& stitchInputFmt)
{
	stitchInputFmt = amf::AMF_SURFACE_NV12;
	//---------------------------------------------------------------------------------------------
	amf_int32 streamCount = (amf_int32)m_Cameras.size();
//	GetParam(StitchPipelineBase::PARAM_NAME_STREAM_COUNT, streamCount);
	amf_int32 compositedWidth = 3840;
	amf_int32 compositedHeight = 1920;
	GetParam(StitchPipelineBase::PARAM_NAME_COMPOSITED_WIDTH, compositedWidth);
	GetParam(StitchPipelineBase::PARAM_NAME_COMPOSITED_HEIGHT, compositedHeight);

	amf_int64 modeZCam = AMF_VIDEO_STITCH_CAM_MODE_INVALID;
	GetParam(PARAM_NAME_ZCAMLIVE_MODE, modeZCam);
	bool isZCamLive = modeZCam != AMF_VIDEO_STITCH_CAM_MODE_INVALID;
	if (isZCamLive && (AMF_VIDEO_STITCH_CAM_MODE_THETAS!= modeZCam) && (AMF_VIDEO_STITCH_CAM_MODE_THETAV != modeZCam))
	{
		
		streamCount = m_pZCamDemuxer->GetOutputCount();
	}

	//---------------------------------------------------------------------------------------------
	// Init Video Stitch
	amf::AMFComponentPtr pStitch;
	AMF_RESULT res = g_AMFFactory.LoadExternalComponent(m_pContext, STITCH_DLL_NAME, "AMFCreateComponentInt", NULL, &pStitch);
	CHECK_AMF_ERROR_RETURN(res, L"AMFCreateComponent(" << AMFStitchLoom << L") failed");
	m_pStitch = amf::AMFComponentExPtr(pStitch);

	m_pStitch->SetProperty(AMF_VIDEO_STITCH_MEMORY_TYPE, m_pVideoPresenter->GetMemoryType());
	m_pStitch->SetProperty(AMF_VIDEO_STITCH_INPUTCOUNT, amf_int64(streamCount));
	m_pStitch->SetProperty(AMF_VIDEO_STITCH_OUTPUT_SIZE, AMFConstructSize(compositedWidth, compositedHeight));
	m_pStitch->SetProperty(AMF_VIDEO_STITCH_FILL, true);
	m_pStitch->SetProperty(AMF_VIDEO_STITCH_FILL_COLOR, ::AMFConstructColor(0, 255, 0, 255));
	UpdateStitchMode();
	return res;
}
AMF_RESULT 
FAmfStitchMediaPipeline::UpdateStitchMode()
{
	if (m_pStitch != nullptr && MediaOutput != nullptr)
	{
        amf::AMF_SURFACE_FORMAT surfaceFormat = amf::AMF_SURFACE_RGBA;
        switch (MediaOutput->GetSinkFormat())
        {
        case EMediaTextureSinkFormat::CharBGRA:
            //MM for compatibility with AMF - temporarely
            surfaceFormat = amf::AMF_SURFACE_RGBA;
            break;
        case EMediaTextureSinkFormat::FloatRGBA:
            surfaceFormat = amf::AMF_SURFACE_RGBA_F16;
            break;
        }
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

AMF_RESULT
FAmfStitchMediaPipeline::CreateVideoPresenter(
	amf::AMF_MEMORY_TYPE Type,
	amf_int32 CompositedWidth,
	amf_int32 CompositedHeight
)
{
	if (Type != amf::AMF_MEMORY_DX11)
	{
		return AMF_NOT_SUPPORTED;
	}

	if (!MediaOutput)
	{
		return AMF_NOT_INITIALIZED;
	}

	m_pVideoPresenter = std::make_shared<FAmfStitchVideoPresenter>(
		*MediaOutput, m_pContext
		);
	m_bVideoPresenterDirectConnect = true;

	MediaOutput->Tracks.SetVideoTrack(
		LOCTEXT("AMF Video Track Name", "Video Track 0"),
		0,
		FIntPoint(CompositedWidth, CompositedHeight),
		0
	);
	return AMF_OK;
}

EMediaState
FAmfStitchMediaPipeline::GetMediaState() const
{
	switch (GetState())
	{
	case PipelineStateNotReady:
		return EMediaState::Closed;
	case PipelineStateReady:
	case PipelineStateEof:
		return EMediaState::Stopped;
	case PipelineStateRunning:
		return m_pVideoPresenter->GetMode() == VideoPresenter::ModePlaying
			? EMediaState::Playing
			: EMediaState::Paused;
	default:
		return EMediaState::Error;
	}
}

amf_pts
FAmfStitchMediaPipeline::GetTime() const
{
	return m_pVideoPresenter ? m_pVideoPresenter->GetCurrentTime() : 0;
}

bool
FAmfStitchMediaPipeline::IsLooping() const
{
	return false;
	/*bool bLooping = false;
	return (AMF_OK == GetParam(PARAM_NAME_LOOP, bLooping))
		? bLooping
		: false;*/
}

bool
FAmfStitchMediaPipeline::SetLooping(bool bLooping)
{
	return false;
	//return AMF_OK == SetParam(PARAM_NAME_LOOP, bLooping);
}

bool
FAmfStitchMediaPipeline::SupportsSeeking() const
{
	return false;
}
AMF_RESULT  FAmfStitchMediaPipeline::Terminate()
{
	return StitchPipelineBase::Terminate();
}
AMF_RESULT FAmfStitchMediaPipeline::CreateAudioPresenter()
{
	return AMF_OK;
}

extern "C"
{
	AMF_RESULT AMF_CDECL_CALL AMFCreateComponentAudioCapture(amf::AMFContext* pContext, amf::AMFComponentEx** ppComponent)
	{
		return AMF_OK;
	}
}


#undef LOCTEXT_NAMESPACE

#endif
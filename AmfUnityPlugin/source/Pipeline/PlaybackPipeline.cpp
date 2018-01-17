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
#include "PlaybackPipeline.h"
#include "VideoPresenterTexture.h"
#include "AudioPresenterBuffer.h"


PlaybackPipeline::PlaybackPipeline()
	: m_device(NULL), m_hasAudio(false), m_ambisonicAudio(false), m_isMuted(false)
{
}

AMF_RESULT
PlaybackPipeline::Init(void* device)
{
	m_device = device;
	//
	return PlaybackPipelineBase::Init();
}

AMF_RESULT
PlaybackPipeline::InitContext(amf::AMF_MEMORY_TYPE type)
{
	g_AMFFactory.GetFactory()->CreateContext(&m_pContext);

	switch (type)
	{
	case amf::AMF_MEMORY_DX9:
		{
			const AMF_RESULT res = m_pContext->InitDX9(m_device);
			CHECK_AMF_ERROR_RETURN(res, "Init DX9");
			return AMF_OK;
		}

	case amf::AMF_MEMORY_DX11:
		{
			const AMF_RESULT res = m_pContext->InitDX11(m_device);
			CHECK_AMF_ERROR_RETURN(res, "Init DX11");
			return AMF_OK;
		}

	case amf::AMF_MEMORY_OPENGL:
		{
			AMF_RESULT res = m_pContext->InitDX9(m_device);
			CHECK_AMF_ERROR_RETURN(res, "Init DX9");
			res = m_pContext->InitOpenGL(NULL, NULL, NULL);
			CHECK_AMF_ERROR_RETURN(res, "Init OpenGL");
			return AMF_OK;
		}

	default:
		return AMF_NOT_SUPPORTED;
	}
}

AMF_RESULT
PlaybackPipeline::CreateVideoPresenter(amf::AMF_MEMORY_TYPE type, amf_int64 bitRate, double fps)
{
	m_pVideoPresenter = std::make_shared<VideoPresenterTexture>();
	return AMF_OK;
}

AMF_RESULT  PlaybackPipeline::InitVideoProcessor()
{
	AMF_RESULT res = g_AMFFactory.GetFactory()->CreateComponent(m_pContext, AMFVideoConverter, &m_pVideoProcessor);
	CHECK_AMF_ERROR_RETURN(res, L"g_AMFFactory.GetFactory()->CreateComponent(" << AMFVideoConverter << L") failed");

	if (m_pVideoPresenter->SupportAllocator() && m_pContext->GetOpenCLContext() == NULL)
	{
		m_pVideoProcessor->SetProperty(AMF_VIDEO_CONVERTER_KEEP_ASPECT_RATIO, true);
		m_pVideoProcessor->SetProperty(AMF_VIDEO_CONVERTER_FILL, true);
		m_pVideoPresenter->SetProcessor(m_pVideoProcessor);
	}

	m_pVideoProcessor->SetProperty(AMF_VIDEO_CONVERTER_MEMORY_TYPE, m_pVideoPresenter->GetMemoryType());
	m_pVideoProcessor->SetProperty(AMF_VIDEO_CONVERTER_COMPUTE_DEVICE, amf::AMF_MEMORY_COMPUTE_FOR_DX11);
	m_pVideoProcessor->SetProperty(AMF_VIDEO_CONVERTER_OUTPUT_FORMAT, m_pVideoPresenter->GetInputFormat());

	m_pVideoProcessor->Init(amf::AMF_SURFACE_NV12, m_iVideoWidth, m_iVideoHeight);
	return AMF_OK;
}

AMF_RESULT
PlaybackPipeline::CreateAudioPresenter()
{
#if defined(_WIN32)
	m_pAudioPresenter = std::make_shared<AudioPresenterBuffer>();
	m_hasAudio = (NULL != m_pAudioPresenter);
	return AMF_OK;
#else
    return AMF_NOT_IMPLEMENTED;
#endif
}

void PlaybackPipeline::Lock()
{
	VideoPresenterTexturePtr ptr = std::dynamic_pointer_cast<VideoPresenterTexture>(m_pVideoPresenter);
	if (ptr)
	{
		ptr->Lock();
	}
}

void PlaybackPipeline::Unlock()
{
	VideoPresenterTexturePtr ptr = std::dynamic_pointer_cast<VideoPresenterTexture>(m_pVideoPresenter);
	if (ptr)
	{
		ptr->Unlock();
	}
}

amf::AMFSurface* PlaybackPipeline::GetSurface()
{
	VideoPresenterTexturePtr ptr = std::dynamic_pointer_cast<VideoPresenterTexture>(m_pVideoPresenter);
	if (ptr)
	{
		return ptr->GetSurface();
	}
	return NULL;
}

unsigned PlaybackPipeline::GetTextureWidth() const
{
	return m_pVideoPresenter->GetFrameWidth();
}

unsigned PlaybackPipeline::GetTextureHeight() const
{
	return m_pVideoPresenter->GetFrameHeight();
}

void PlaybackPipeline::GetAudioData(float* audioOut, int outSize) const
{
	if (m_pAudioPresenter)
	{
		static_cast<AudioPresenterBuffer*>(m_pAudioPresenter.get())->Lock();
		static_cast<AudioPresenterBuffer*>(m_pAudioPresenter.get())->StoreNewAudio(audioOut, outSize);
		static_cast<AudioPresenterBuffer*>(m_pAudioPresenter.get())->Unlock();
	}
}

void PlaybackPipeline::SetAmbisonicAudio(bool isAmbisonic)
{
	m_ambisonicAudio = isAmbisonic;
}

void PlaybackPipeline::SetAmbisonicAngles(float theta, float phi, float rho)
{
	if (m_pAudioPresenter)
	{
		if (m_ambisonicAudio)
		{
			m_pAmbisonicRender->SetProperty(AMF_AMBISONIC2SRENDERER_THETA, theta);
			m_pAmbisonicRender->SetProperty(AMF_AMBISONIC2SRENDERER_PHI, phi);
			m_pAmbisonicRender->SetProperty(AMF_AMBISONIC2SRENDERER_RHO, rho);
		}
	}
}

unsigned PlaybackPipeline::GetSampleRate() const
{
	if (m_pAudioPresenter)
	{
			return static_cast<AudioPresenterBuffer*>(m_pAudioPresenter.get())->GetSampleRate();
	}
	return 0;
}

unsigned PlaybackPipeline::GetChannels() const
{
	if (m_pAudioPresenter)
	{
		return static_cast<AudioPresenterBuffer*>(m_pAudioPresenter.get())->GetChannels();
	}
	return 0;
}

bool PlaybackPipeline::HasAudio() const
{
	return m_hasAudio;
}

bool PlaybackPipeline::isAmbisonic() const
{
	return m_ambisonicAudio;
}

void PlaybackPipeline::Mute(bool mute)
{
	m_isMuted = mute;
}

AMF_RESULT PlaybackPipeline::InitAudioPipeline(amf_uint32 iAudioStreamIndex, PipelineElementPtr pAudioSourceStream)
{
	if (iAudioStreamIndex >= 0 && m_pAudioPresenter != NULL && pAudioSourceStream != NULL && !m_isMuted)
	{
		Connect(PipelineElementPtr(new AMFComponentElement(m_pAudioDecoder)), 0, pAudioSourceStream, iAudioStreamIndex, m_bURL ? 1000 : 100, CT_ThreadQueue);
		if (m_ambisonicAudio)
		{
			Connect(PipelineElementPtr(new AMFComponentElement(m_pAmbisonicRender)), 0, CT_Direct);
		}
		Connect(PipelineElementPtr(new AMFComponentElement(m_pAudioConverter)), 0, CT_Direct);
		Connect(m_pAudioPresenter, 0, CT_Direct);
	}
	return AMF_OK;
}

AMF_RESULT  PlaybackPipeline::InitAudio(amf::AMFOutput* pOutput)
{

	if (m_isMuted)
	{
		return AMF_OK;
	}
	AMF_RESULT res = AMF_OK;

	amf_int64 codecID = 0;
	amf_int64 streamBitRate = 0;
	amf_int64 streamSampleRate = 0;
	amf_int64 streamChannels = 0;
	amf_int64 streamFormat = 0;
	amf_int64 streamLayout = 0;
	amf_int64 streamBlockAlign = 0;
	amf_int64 streamFrameSize = 0;
	amf::AMFInterfacePtr pExtradata;

	//extract initial information from demuxer
	pOutput->GetProperty(FFMPEG_DEMUXER_CODEC_ID, &codecID);

	pOutput->GetProperty(FFMPEG_DEMUXER_BIT_RATE, &streamBitRate);
	pOutput->GetProperty(FFMPEG_DEMUXER_EXTRA_DATA, &pExtradata);

	pOutput->GetProperty(FFMPEG_DEMUXER_AUDIO_SAMPLE_RATE, &streamSampleRate);
	pOutput->GetProperty(FFMPEG_DEMUXER_AUDIO_CHANNELS, &streamChannels);
	pOutput->GetProperty(FFMPEG_DEMUXER_AUDIO_SAMPLE_FORMAT, &streamFormat);
	pOutput->GetProperty(FFMPEG_DEMUXER_AUDIO_CHANNEL_LAYOUT, &streamLayout);
	pOutput->GetProperty(FFMPEG_DEMUXER_AUDIO_BLOCK_ALIGN, &streamBlockAlign);
	pOutput->GetProperty(FFMPEG_DEMUXER_AUDIO_FRAME_SIZE, &streamFrameSize);

	if (m_ambisonicAudio)
	{
		CHECK_RETURN(4 == streamChannels, AMF_FAIL, L"Ambisonic renderer need exactly 4 channels");
	}

	CHECK_AMF_ERROR_RETURN(res, L"g_AMFFactory.LoadExternalComponent(AmbisonicRender) failed, AUDIO is in default mode");

	//get audio decoder
	res = g_AMFFactory.LoadExternalComponent(m_pContext, FFMPEG_DLL_NAME, "AMFCreateComponentInt", FFMPEG_AUDIO_DECODER, &m_pAudioDecoder);
	CHECK_AMF_ERROR_RETURN(res, L"LoadExternalComponent(" << FFMPEG_AUDIO_DECODER << L") failed");
	++m_nFfmpegRefCount;

	//program audi decoder
	m_pAudioDecoder->SetProperty(AUDIO_DECODER_IN_AUDIO_CODEC_ID, codecID);
	m_pAudioDecoder->SetProperty(AUDIO_DECODER_IN_AUDIO_BIT_RATE, streamBitRate);
	m_pAudioDecoder->SetProperty(AUDIO_DECODER_IN_AUDIO_EXTRA_DATA, pExtradata);
	m_pAudioDecoder->SetProperty(AUDIO_DECODER_IN_AUDIO_SAMPLE_RATE, streamSampleRate);
	m_pAudioDecoder->SetProperty(AUDIO_DECODER_IN_AUDIO_CHANNELS, streamChannels);
	m_pAudioDecoder->SetProperty(AUDIO_DECODER_IN_AUDIO_SAMPLE_FORMAT, streamFormat);
	m_pAudioDecoder->SetProperty(AUDIO_DECODER_IN_AUDIO_CHANNEL_LAYOUT, streamLayout);
	m_pAudioDecoder->SetProperty(AUDIO_DECODER_IN_AUDIO_BLOCK_ALIGN, streamBlockAlign);
	m_pAudioDecoder->SetProperty(AUDIO_DECODER_IN_AUDIO_FRAME_SIZE, streamFrameSize);
	//initialize audio decoder
	res = m_pAudioDecoder->Init(amf::AMF_SURFACE_UNKNOWN, 0, 0);
	CHECK_AMF_ERROR_RETURN(res, L"m_pAudioDecoder->Init() failed");

	res = CreateAudioPresenter();
	CHECK_AMF_ERROR_RETURN(res, "Failed to create an audio presenter");
	m_pAudioPresenter->SetLowLatency(true);
	res = m_pAudioPresenter->Init();
	CHECK_AMF_ERROR_RETURN(res, L"m_pAudioPresenter->Init() failed");


	//extract needed information from audio decoder
	m_pAudioDecoder->GetProperty(AUDIO_DECODER_OUT_AUDIO_BIT_RATE, &streamBitRate);
	m_pAudioDecoder->GetProperty(AUDIO_DECODER_OUT_AUDIO_SAMPLE_RATE, &streamSampleRate);
	m_pAudioDecoder->GetProperty(AUDIO_DECODER_OUT_AUDIO_CHANNELS, &streamChannels);
	m_pAudioDecoder->GetProperty(AUDIO_DECODER_OUT_AUDIO_SAMPLE_FORMAT, &streamFormat);
	m_pAudioDecoder->GetProperty(AUDIO_DECODER_OUT_AUDIO_CHANNEL_LAYOUT, &streamLayout);
	m_pAudioDecoder->GetProperty(AUDIO_DECODER_OUT_AUDIO_BLOCK_ALIGN, &streamBlockAlign);

	// if using ambisonic audio component
	if (m_ambisonicAudio)
	{
		res = AMFCreateComponentAmbisonic(m_pContext, NULL, &m_pAmbisonicRender);

		//program Ambisonic Render (output)

		m_pAmbisonicRender->GetProperty(AMF_AMBISONIC2SRENDERER_OUT_AUDIO_CHANNELS, &streamChannels);
		m_pAmbisonicRender->GetProperty(AMF_AMBISONIC2SRENDERER_OUT_AUDIO_SAMPLE_FORMAT, &streamFormat);
		m_pAmbisonicRender->GetProperty(AMF_AMBISONIC2SRENDERER_OUT_AUDIO_CHANNEL_LAYOUT, &streamLayout);

		//program Ambisonic Render (input)
		m_pAmbisonicRender->SetProperty(AMF_AMBISONIC2SRENDERER_IN_AUDIO_CHANNELS, streamChannels);
		m_pAmbisonicRender->SetProperty(AMF_AMBISONIC2SRENDERER_IN_AUDIO_SAMPLE_FORMAT, streamFormat);
		m_pAmbisonicRender->SetProperty(AMF_AMBISONIC2SRENDERER_IN_AUDIO_SAMPLE_RATE, streamSampleRate);

		m_pAmbisonicRender->SetProperty(AMF_AMBISONIC2SRENDERER_W, 0);
		m_pAmbisonicRender->SetProperty(AMF_AMBISONIC2SRENDERER_X, 1);
		m_pAmbisonicRender->SetProperty(AMF_AMBISONIC2SRENDERER_Y, 3);
		m_pAmbisonicRender->SetProperty(AMF_AMBISONIC2SRENDERER_Z, 2);

		amf_int64 val;
		m_pAmbisonicRender->GetProperty(AMF_AMBISONIC2SRENDERER_W, &val);
		GetParam(AMF_AMBISONIC2SRENDERER_W, val);
		m_pAmbisonicRender->SetProperty(AMF_AMBISONIC2SRENDERER_W, val);

		m_pAmbisonicRender->GetProperty(AMF_AMBISONIC2SRENDERER_X, &val);
		GetParam(AMF_AMBISONIC2SRENDERER_X, val);
		m_pAmbisonicRender->SetProperty(AMF_AMBISONIC2SRENDERER_X, val);

		m_pAmbisonicRender->GetProperty(AMF_AMBISONIC2SRENDERER_Y, &val);
		GetParam(AMF_AMBISONIC2SRENDERER_Y, val);
		m_pAmbisonicRender->SetProperty(AMF_AMBISONIC2SRENDERER_Y, val);

		m_pAmbisonicRender->GetProperty(AMF_AMBISONIC2SRENDERER_Z, &val);
		GetParam(AMF_AMBISONIC2SRENDERER_Z, val);
		m_pAmbisonicRender->SetProperty(AMF_AMBISONIC2SRENDERER_Z, val);

		// dynamic properties
		m_pAmbisonicRender->SetProperty(AMF_AMBISONIC2SRENDERER_THETA, 0.0);
		m_pAmbisonicRender->SetProperty(AMF_AMBISONIC2SRENDERER_PHI, 0.0);
		m_pAmbisonicRender->SetProperty(AMF_AMBISONIC2SRENDERER_RHO, 0.0);

		res = m_pAmbisonicRender->Init(amf::AMF_SURFACE_UNKNOWN, 0, 0);
		CHECK_AMF_ERROR_RETURN(res, L"m_pAmbisonicRender->Init() failed");
	} // if m_ambisonicAudio

	// insert aaudio converter
	res = g_AMFFactory.LoadExternalComponent(m_pContext, FFMPEG_DLL_NAME, "AMFCreateComponentInt", FFMPEG_AUDIO_CONVERTER, &m_pAudioConverter);
	CHECK_AMF_ERROR_RETURN(res, L"LoadExternalComponent(" << FFMPEG_AUDIO_CONVERTER << L") failed");
	++m_nFfmpegRefCount;


	m_pAudioConverter->SetProperty(AUDIO_CONVERTER_IN_AUDIO_BIT_RATE, streamBitRate);
	m_pAudioConverter->SetProperty(AUDIO_CONVERTER_IN_AUDIO_SAMPLE_RATE, streamSampleRate);
	m_pAudioConverter->SetProperty(AUDIO_CONVERTER_IN_AUDIO_CHANNELS, streamChannels);
	m_pAudioConverter->SetProperty(AUDIO_CONVERTER_IN_AUDIO_SAMPLE_FORMAT, streamFormat);
	m_pAudioConverter->SetProperty(AUDIO_CONVERTER_IN_AUDIO_CHANNEL_LAYOUT, streamLayout);
	m_pAudioConverter->SetProperty(AUDIO_CONVERTER_IN_AUDIO_BLOCK_ALIGN, streamBlockAlign);

	m_pAudioPresenter->GetDescription(
		streamBitRate,
		streamSampleRate,
		streamChannels,
		streamFormat,
		streamLayout,
		streamBlockAlign
		);


	m_pAudioConverter->SetProperty(AUDIO_CONVERTER_OUT_AUDIO_BIT_RATE, streamBitRate);
	m_pAudioConverter->SetProperty(AUDIO_CONVERTER_OUT_AUDIO_SAMPLE_RATE, streamSampleRate);
	m_pAudioConverter->SetProperty(AUDIO_CONVERTER_OUT_AUDIO_CHANNELS, streamChannels);
	m_pAudioConverter->SetProperty(AUDIO_CONVERTER_OUT_AUDIO_SAMPLE_FORMAT, streamFormat);
	m_pAudioConverter->SetProperty(AUDIO_CONVERTER_OUT_AUDIO_CHANNEL_LAYOUT, streamLayout);
	m_pAudioConverter->SetProperty(AUDIO_CONVERTER_OUT_AUDIO_BLOCK_ALIGN, streamBlockAlign);

	res = m_pAudioConverter->Init(amf::AMF_SURFACE_UNKNOWN, 0, 0);
	CHECK_AMF_ERROR_RETURN(res, L"m_pAudioConverter->Init() failed");

	pOutput->SetProperty(FFMPEG_DEMUXER_STREAM_ENABLED, true);

	return AMF_OK;
}

AMF_RESULT PlaybackPipeline::Stop()
{

	PlaybackPipelineBase::Stop();

	if (m_pAmbisonicRender != NULL)
	{
		m_pAmbisonicRender.Release();
		m_pAmbisonicRender = NULL;
	}

	return AMF_OK;
}

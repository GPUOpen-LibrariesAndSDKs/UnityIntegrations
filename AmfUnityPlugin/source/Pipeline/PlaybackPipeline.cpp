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
	: m_device(NULL), m_hasAudio(false)
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
		static_cast<AudioPresenterBuffer*>(m_pAudioPresenter.get())->StoreNewAudio(audioOut, outSize);
	}
}

unsigned PlaybackPipeline::GetSampleRate() const
{
	return static_cast<AudioPresenterBuffer*>(m_pAudioPresenter.get())->GetSampleRate();
}

unsigned PlaybackPipeline::GetChannels() const
{
	return static_cast<AudioPresenterBuffer*>(m_pAudioPresenter.get())->GetChannels();
}

bool PlaybackPipeline::HasAudio() const
{
	return m_hasAudio;
}

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
#include <d3d11.h>

#include "VideoPresenterTexture.h"


VideoPresenterTexture::VideoPresenterTexture() :
	VideoPresenter(),
	m_initialized(false),
	m_surf(NULL)
{
}

VideoPresenterTexture::~VideoPresenterTexture()
{
	Terminate();
}

AMF_RESULT VideoPresenterTexture::Present(amf::AMFSurface* pSurface)
{
	AMF_RESULT err = AMF_OK;
	m_sect.Lock();
	// We save our surface for later use by Unity
	m_surf = pSurface;
	m_sect.Unlock();
	WaitForPTS(pSurface->GetPts());

	return AMF_OK;
}

AMF_RESULT VideoPresenterTexture::Init(amf_int32 width, amf_int32 height)
{
	AMF_RESULT res = AMF_OK;

	VideoPresenter::Init(width, height);
	m_initialized = true;
	return res;
}

AMF_RESULT VideoPresenterTexture::Terminate()
{
	return VideoPresenter::Terminate();
}

AMF_RESULT AMF_STD_CALL VideoPresenterTexture::AllocSurface(amf::AMF_MEMORY_TYPE type, amf::AMF_SURFACE_FORMAT format,
	amf_int32 width, amf_int32 height, amf_int32 hPitch, amf_int32 vPitch, amf::AMFSurface** ppSurface)
{
	return AMF_OK;
}

void AMF_STD_CALL VideoPresenterTexture::OnSurfaceDataRelease(amf::AMFSurface* pSurface)
{
}

void VideoPresenterTexture::Lock()
{
	m_sect.Lock();
}

void VideoPresenterTexture::Unlock()
{
	// Clear the surface and unlock
	m_surf = NULL;
	m_sect.Unlock();
}

amf::AMFSurfacePtr VideoPresenterTexture::GetSurface()
{
	return m_surf;
}

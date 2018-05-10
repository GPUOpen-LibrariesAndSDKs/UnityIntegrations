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

#pragma once

// Unreal includes
#include "IMediaControls.h"
#include "UniquePtr.h"

// AMF includes
#include "BitStreamParser.h"
#include "ParametersStorage.h"
#include "StitchPipelineBase.h"
#include "RHI.h"

#ifdef AMD_MGPU_SUPPORTED
//	#undef AMD_MGPU_SUPPORTED
#endif 

struct ID3D11Device;
class FAmfStitchMediaOutput;
class FVRContext;

class FAmfStitchMediaPipeline
	: public StitchPipelineBase
{
public:
	FAmfStitchMediaPipeline();

	AMF_RESULT InitStitch(
		FAmfStitchMediaOutput*,
		const std::vector<std::wstring>& StreamFilePaths,
		const std::wstring& PtguiFilePath
	);
	virtual AMF_RESULT  Terminate() override;

	bool IsLooping() const;
	bool SetLooping(bool bLooping);

	amf_pts GetTime() const;

	/** Get the state in Unreal terms */
	EMediaState GetMediaState() const;

	bool SupportsSeeking() const;
	AMF_RESULT UpdateStitchMode();

protected:
	AMF_RESULT InitContext(amf::AMF_MEMORY_TYPE) override;
	AMF_RESULT InitStitcher(amf::AMF_SURFACE_FORMAT&) override;
	AMF_RESULT CreateVideoPresenter(amf::AMF_MEMORY_TYPE, amf_int32 CompositedWidth, amf_int32 CompositedHeight) override;
	AMF_RESULT CreateAudioPresenter() override;

private:
	FAmfStitchMediaOutput*	MediaOutput;
	TRefCountPtr<ID3D11Device> AmfDevice;
#ifdef 	AMD_MGPU_SUPPORTED
	TSharedPtr<FVRContext>		VRContext;
#endif
};
#endif
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
#pragma once

#if 0

// Unreal includes
#include "TickableObjectRenderThread.h"
#include "Tickable.h"
#include "RHI.h"

#include "VideoPresenter.h"


class FAmfStitchMediaOutput;

struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11Query;
struct ID3D11Resource;
struct ID3D11Texture2D;

// The presenter must create an instance of this class on the render thread
// so that it can be ticked on the render thread to update the sink texture.


class FAmfStitchVideoUpdater
	: public FTickableObjectRenderThread
{
public:
	FAmfStitchVideoUpdater(FAmfStitchMediaOutput&
		, amf::AMFContext* InContextAMF
	);
	void SubmitSurface(amf::AMFSurface*);

	//~ FTickableObjectRenderThread interface
	TStatId GetStatId() const override;
	bool IsTickable() const override;
	void Tick(float DeltaTime) override;

	// amf::AMFDataAllocatorCB interface
	virtual AMF_RESULT AMF_STD_CALL AllocSurface(amf::AMF_MEMORY_TYPE type, amf::AMF_SURFACE_FORMAT format,
		amf_int32 width, amf_int32 height, amf_int32 hPitch, amf_int32 vPitch, amf::AMFSurface** ppSurface);


private:
    void FreeSubmittedSurface();

	FAmfStitchMediaOutput& MediaOutput;
	amf::AMFSurfacePtr			SubmittedSurface;
	amf::AMFSurfacePtr			CopiedSurface;

	TRefCountPtr<ID3D11Query>	CopyEventQueryGPU1;
	volatile bool						EndCopyEventQueryGPU1;

	mutable FCriticalSection CriticalSection;
	struct MyTexture
	{
		FTextureRHIRef					TextureRHI;
		TRefCountPtr<ID3D11Texture2D>	TextureAMF;
		amf_int32						SizeX;
	};

	TQueue< MyTexture >						FreeList;
	TDoubleLinkedList< MyTexture >			AllocatedList;
	
	MyTexture								CurrentTexture;

	TRefCountPtr<ID3D11Device>				AmfDevice;
	amf::AMFContextPtr						ContextAMF;
};

class FAmfStitchVideoPresenter
	: public VideoPresenter
{
public:
	FAmfStitchVideoPresenter(
		FAmfStitchMediaOutput&, 
		amf::AMFContext* pContextAMF
	);
	~FAmfStitchVideoPresenter() override;

	//~ VideoPresenter interface
	AMF_RESULT Present(amf::AMFSurface*) override;
	amf::AMF_MEMORY_TYPE GetMemoryType() const override { return amf::AMF_MEMORY_DX11; }
	amf::AMF_SURFACE_FORMAT GetInputFormat() const override { return amf::AMF_SURFACE_BGRA; }
	
	AMF_RESULT SetInputFormat(amf::AMF_SURFACE_FORMAT Format) override
	{
		return Format == amf::AMF_SURFACE_BGRA ? AMF_OK : AMF_FAIL;
	}

	bool SupportAllocator() const override { return true; }

	AMF_RESULT Terminate() override;

	// amf::AMFDataAllocatorCB interface
	virtual AMF_RESULT AMF_STD_CALL AllocSurface(amf::AMF_MEMORY_TYPE type, amf::AMF_SURFACE_FORMAT format,
		amf_int32 width, amf_int32 height, amf_int32 hPitch, amf_int32 vPitch, amf::AMFSurface** ppSurface);

private:
    FAmfStitchMediaOutput& MediaOutput;

	TUniquePtr<FAmfStitchVideoUpdater> Updater;

	TRefCountPtr<ID3D11DeviceContext>	AmfDeviceContext;

	// Used to wait until the AMF device has finished stitching
	TRefCountPtr<ID3D11Query>	AmfEventQuery;
};

#endif
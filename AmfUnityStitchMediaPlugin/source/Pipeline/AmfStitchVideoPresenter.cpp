// Copyright (c) 2017 Advanced Micro Devices, Inc. All rights reserved.
#if 0

#include "AmfStitchMediaPCH.h"
#include "AmfStitchVideoPresenter.h"
#include "AmfStitchMediaOutput.h"

#include "RenderingThread.h"
#include "RHI.h"
#include "SceneManagement.h"

#include "AllowWindowsPlatformTypes.h"
#include <D3D11.h>
#include "HideWindowsPlatformTypes.h"

#include "public/include/components/VideoConverter.h"
#include "public/common/TraceAdapter.h"

FAmfStitchVideoPresenter::FAmfStitchVideoPresenter(
	FAmfStitchMediaOutput& MediaOutputIn,
	amf::AMFContext* InContextAMF
) :
    MediaOutput(MediaOutputIn)
{
	ID3D11Device* AmfDevice = (ID3D11Device*)InContextAMF->GetDX11Device();
	AmfDevice->GetImmediateContext(AmfDeviceContext.GetInitReference());

	D3D11_QUERY_DESC QueryDesc = {};
	QueryDesc.Query = D3D11_QUERY_EVENT;
	QueryDesc.MiscFlags = 0;

	AmfDevice->CreateQuery(&QueryDesc, AmfEventQuery.GetInitReference());

	ENQUEUE_UNIQUE_RENDER_COMMAND_THREEPARAMETER(
		CreateAmfVideoUpdater,
		FAmfStitchVideoPresenter*, This, this,
		FAmfStitchMediaOutput&, MediaOutputIn, MediaOutputIn,
		amf::AMFContextPtr, InContextAMF, InContextAMF,
	{
			This->Updater.Reset(
				new FAmfStitchVideoUpdater(MediaOutputIn, InContextAMF)
			);
		}
	);

	FlushRenderingCommands();
}

FAmfStitchVideoPresenter::~FAmfStitchVideoPresenter()
{
	Terminate();
}

AMF_RESULT
FAmfStitchVideoPresenter::Terminate()
{
	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
		DeleteAmfStitchVideoUpdater,
		FAmfStitchVideoPresenter*, This, this,
		{
			This->Updater = nullptr;
		}
	);

	FlushRenderingCommands();

	return VideoPresenter::Terminate();
}

FAmfStitchVideoUpdater::FAmfStitchVideoUpdater(
	FAmfStitchMediaOutput& InMediaOutput
	, amf::AMFContext* InContextAMF
)
	: FTickableObjectRenderThread(true, true)
	, MediaOutput(InMediaOutput)
	, EndCopyEventQueryGPU1(false)
	, ContextAMF(InContextAMF)
{
	AmfDevice = (ID3D11Device*)ContextAMF->GetDX11Device();

	ID3D11Device* EngineDevice = static_cast<ID3D11Device*>(GDynamicRHI->RHIGetNativeDevice());
	check(EngineDevice != nullptr);
	
	TRefCountPtr<ID3D11DeviceContext> EngineDeviceContext;
	EngineDevice->GetImmediateContext(EngineDeviceContext.GetInitReference());

	D3D11_QUERY_DESC QueryDesc = {};
	QueryDesc.Query = D3D11_QUERY_EVENT;
	QueryDesc.MiscFlags = 0;

	EngineDevice->CreateQuery(&QueryDesc, CopyEventQueryGPU1.GetInitReference());
}

TStatId
FAmfStitchVideoUpdater::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(FAmfStitchVideoUpdater, STATGROUP_Tickables);
}

bool
FAmfStitchVideoUpdater::IsTickable() const
{
	FScopeLock Lock(&CriticalSection);
	return SubmittedSurface != nullptr || CopiedSurface != nullptr ;
}

void FAmfStitchVideoUpdater::FreeSubmittedSurface()
{
    FScopeLock Lock(&CriticalSection);
    if (SubmittedSurface != NULL)
    {
        ID3D11Texture2D* NativeResource = (ID3D11Texture2D*)SubmittedSurface->GetPlaneAt(0)->GetNative();

        TDoubleLinkedList< MyTexture >::TDoubleLinkedListNode* Node = AllocatedList.GetHead();
        while (Node != nullptr)
        {
            if (Node->GetValue().TextureAMF == NativeResource)
            {
                break;
            }
            Node = Node->GetNextNode();
        }
        if (Node == nullptr)
        {
            AMFTraceWarning(L"AAA", L"Surface not found - something is wrong");
            return;
        }
        
        if (Node->GetValue().TextureAMF != nullptr)
        {

            if (Node->GetValue().SizeX != 1)
            {
                FreeList.Enqueue(Node->GetValue());
            }
        }
        AllocatedList.RemoveNode(Node);

    }
    SubmittedSurface = nullptr;

}

void
FAmfStitchVideoUpdater::SubmitSurface(amf::AMFSurface* InAmfSurface)
{
	//MM to test 
	/*
	while (true)
	{
		{
			FScopeLock Lock(&CriticalSection);
			if (!IsTickable())
			{
				break;
			}
		}
		Sleep(1);
	}
	*/
	// end of test
	FScopeLock Lock(&CriticalSection);
    FreeSubmittedSurface();
	SubmittedSurface = InAmfSurface;
}

void
FAmfStitchVideoUpdater::Tick(float /*DeltaTime*/)
{
	ID3D11Device* EngineDevice = static_cast<ID3D11Device*>(GDynamicRHI->RHIGetNativeDevice());
	check(EngineDevice != nullptr);

	TRefCountPtr<ID3D11DeviceContext> EngineDeviceContext;
	EngineDevice->GetImmediateContext(EngineDeviceContext.GetInitReference());
	check(EngineDeviceContext != nullptr);

	FScopeLock Lock(&CriticalSection);

	IMediaTextureSink* Sink = MediaOutput.GetVideoSink();
	if (!Sink)
	{
		return;
	}

	FRHITexture* TextureRHI = Sink->GetTextureSinkTexture();
	if (!TextureRHI)
	{
		return;
	}
#if defined(USE_CUBEMAP_MEDIATEXTURE)
    FRHITextureCube* TextureCubeRHI = TextureRHI->GetTextureCube();
#endif
	FRHITexture2D* Texture2DRHI = TextureRHI->GetTexture2D();
	ID3D11Texture2D* pTextureDst = NULL;

	if (Texture2DRHI)
	{
		if (Texture2DRHI->GetSizeX() == 1 && Texture2DRHI->GetSizeY() == 1 && SubmittedSurface == nullptr)
		{
			return;
		}
		pTextureDst = static_cast<ID3D11Texture2D*>(Texture2DRHI->GetNativeResource());
	}
#if defined(USE_CUBEMAP_MEDIATEXTURE)
    if (TextureCubeRHI)
	{
		if (TextureCubeRHI->GetSize() == 1 && SubmittedSurface == nullptr)
		{
			return;
		}
		pTextureDst = static_cast<ID3D11Texture2D*>(TextureCubeRHI->GetNativeResource());
	}
#endif
	if (CopiedSurface)
	{
		// replace ready texture
		if (!EndCopyEventQueryGPU1)
		{
			BOOL bQueryData = 0;
			HRESULT QueryResult = EngineDeviceContext->GetData(CopyEventQueryGPU1, &bQueryData, sizeof(bQueryData), 0);
			if (QueryResult != S_OK || bQueryData != 1)
			{
				AMFTraceWarning(L"FAmfStitchVideoUpdater", L"GPU1 is not ready");
				return;
			}
			EndCopyEventQueryGPU1 = true;
		}

		CopiedSurface = nullptr;
	}
#if 0 // MM TODO make it working
	// if copy finished
	if (CopiedSurface != nullptr)
	{
		bool bCopyComplete = false;
		if (!EndCopyEventQueryGPU1)
		{
			BOOL bQueryData = 0;
			HRESULT QueryResult = EngineDeviceContext->GetData(CopyEventQueryGPU1, &bQueryData, sizeof(bQueryData), 0);
			if (QueryResult != S_OK || bQueryData != 1)
			{
				AMFTraceWarning(L"AAA", L"GPU1 is not ready");
				return;
			}
			EndCopyEventQueryGPU1 = true;
		}
		CopiedSurface = nullptr;
	}
#endif

	// submit copy to backbuffer
	if (SubmittedSurface)
	{
		ID3D11Texture2D* NativeResource = (ID3D11Texture2D*)SubmittedSurface->GetPlaneAt(0)->GetNative();

		TDoubleLinkedList< MyTexture >::TDoubleLinkedListNode* Node = AllocatedList.GetHead();
		while (Node != nullptr)
		{
			if (Node->GetValue().TextureAMF == NativeResource)
			{
				break;
			}
			Node = Node->GetNextNode();
		}
		if (Node == nullptr)
		{
			AMFTraceWarning(L"AAA", L"Surface not found - something is wrong");
			return;
		}
		if (CurrentTexture.TextureAMF != nullptr)
		{

			if (CurrentTexture.SizeX != 1)
			{
				FreeList.Enqueue(CurrentTexture);
			}
		}
		CurrentTexture = Node->GetValue();
		AllocatedList.RemoveNode(Node);
		if (CurrentTexture.SizeX != 1)
		{
			Sink->UpdateTextureSinkResource(CurrentTexture.TextureRHI, CurrentTexture.TextureRHI);
		}
		SubmittedSurface = nullptr;

#if 0

		TRefCountPtr<IDXGIResource>	DxgiResource;
		NativeResource->QueryInterface(IID_PPV_ARGS(DxgiResource.GetInitReference()));
		check(DxgiResource != nullptr);

		HANDLE SharedHandle = nullptr;
		DxgiResource->GetSharedHandle(&SharedHandle);
		check(SharedHandle != nullptr);

		TRefCountPtr<ID3D11Resource> SharedResource;
		EngineDevice->OpenSharedResource(SharedHandle, IID_PPV_ARGS(SharedResource.GetInitReference()));
		check(SharedResource != nullptr);


		{
			TRefCountPtr<ID3D11Texture2D>	src;
			TRefCountPtr<ID3D11Texture2D>	dst;

			pTextureDst->QueryInterface(IID_PPV_ARGS(dst.GetInitReference()));
			SharedResource->QueryInterface(IID_PPV_ARGS(src.GetInitReference()));

			D3D11_TEXTURE2D_DESC dstDesc = {};
			D3D11_TEXTURE2D_DESC srcDesc = {};

			dst->GetDesc(&dstDesc);
			src->GetDesc(&srcDesc);

//			EngineDeviceContext->CopyResource(pTextureDst,SharedResource);


#if defined(USE_CUBEMAP_MEDIATEXTURE)

			if (TextureCubeRHI != NULL)
			{
				/*
				D3D11_RENDER_TARGET_VIEW_DESC RenderTargetViewDescription = {};
				RenderTargetViewDescription.Format = dstDesc.Format;

				TRefCountPtr<ID3D11RenderTargetView> pRenderTargetView;

				RenderTargetViewDescription.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
				RenderTargetViewDescription.Texture2DArray.ArraySize = 6;
				RenderTargetViewDescription.Texture2DArray.FirstArraySlice = 0;
				RenderTargetViewDescription.Texture2DArray.MipSlice = 0;
				HRESULT hr = EngineDevice->CreateRenderTargetView(dst, &RenderTargetViewDescription, pRenderTargetView.GetInitReference());

				float ClearColor[4] = { 1.0f, 0.0f, 0.0f, 1.0f };
				EngineDeviceContext->ClearRenderTargetView(pRenderTargetView, ClearColor);
				*/

//				EngineDeviceContext->CopyResource(pTextureDst,SharedResource);

				RHICreateTextureCube()
				FRHITexture* NewTextureRHI;
				Sink->UpdateTextureSinkResource();

			}
			else
#endif
			{
				EngineDeviceContext->CopyResource(pTextureDst,SharedResource);
			}

			EndCopyEventQueryGPU1 = false;
			EngineDeviceContext->End(CopyEventQueryGPU1);

			CopiedSurface = SubmittedSurface;
			SubmittedSurface = nullptr;
			ISkylightUpdater * SkylightUpdater = MediaOutput.GetSkylightUpdater();
			if (SkylightUpdater != nullptr)
			{
				SkylightUpdater->Update();
			}
		}
#endif //0
	}
}

AMF_RESULT
FAmfStitchVideoPresenter::Present(amf::AMFSurface* AmfSurface)
{
	AmfDeviceContext->End(AmfEventQuery);

	while (true)
	{
		BOOL bQueryData = 0;
		const HRESULT QueryResult = AmfDeviceContext->GetData(
			AmfEventQuery, &bQueryData, sizeof(bQueryData), 0
		);

		if (QueryResult == S_OK && bQueryData == 1)
		{
			break;
		}

		Sleep(0.0);
	}
	WaitForPTS(AmfSurface->GetPts(), true);
	Updater->SubmitSurface(AmfSurface);

	return AMF_OK;
}

AMF_RESULT AMF_STD_CALL FAmfStitchVideoPresenter::AllocSurface(amf::AMF_MEMORY_TYPE type, amf::AMF_SURFACE_FORMAT format,
	amf_int32 width, amf_int32 height, amf_int32 hPitch, amf_int32 vPitch, amf::AMFSurface** ppSurface)
{
	return Updater->AllocSurface(type, format, width, height, hPitch, vPitch, ppSurface);
}


AMF_RESULT AMF_STD_CALL FAmfStitchVideoUpdater::AllocSurface(amf::AMF_MEMORY_TYPE type, amf::AMF_SURFACE_FORMAT format,
	amf_int32 width, amf_int32 height, amf_int32 hPitch, amf_int32 vPitch, amf::AMFSurface** ppSurface)
{
	FScopeLock Lock(&CriticalSection);

	IMediaTextureSink* Sink = MediaOutput.GetVideoSink();
	if (!Sink)
	{
		return AMF_FAIL;
	}

#if defined(USE_CUBEMAP_MEDIATEXTURE)
    bool bCubemap = Sink->IsCubemap();
#endif
	{
		MyTexture Texture;

		if (FreeList.IsEmpty())
		{
			FRHIResourceCreateInfo CreateInfo;
//            FRHIResourceCreateInfo CreateInfo = { FClearValueBinding(FLinearColor::Red) };

			FIntPoint size = Sink->GetTextureSinkDimensions();
			ID3D11Texture2D* pTextureUE = nullptr;

            uint8 Format = PF_R8G8B8A8;
            switch (MediaOutput.GetSinkFormat())
            {
            case EMediaTextureSinkFormat::CharBGRA:
                //MM for compatibility with AMF - temporarely
                //		BufferFormat = PF_B8G8R8A8;
                Format = PF_R8G8B8A8;
                break;
            case EMediaTextureSinkFormat::FloatRGBA:
                Format = PF_FloatRGBA;
                break;
            }

#if defined(USE_CUBEMAP_MEDIATEXTURE)
            if (bCubemap)
			{
				TRefCountPtr<FRHITextureCube> TextureCubeRHINew;
				FTextureCubeRHIRef RenderTargetTextureRHINew;


				RHICreateTargetableShaderResourceCube(
					size.GetMax(),
                    Format, //    PF_FloatRGBA, 
					1,
					TexCreate_Dynamic | TexCreate_Shared | TexCreate_SRGB, // | (Owner.SRGB ? TexCreate_SRGB : 0) ,
					TexCreate_RenderTargetable,
					false,
					CreateInfo,
					RenderTargetTextureRHINew,
					TextureCubeRHINew
				);
				Texture.TextureRHI = (FTextureRHIRef&)TextureCubeRHINew;
				Texture.SizeX = size.GetMax();
				FRHITextureCube* TextureCubeRHI = Texture.TextureRHI->GetTextureCube();
				pTextureUE = static_cast<ID3D11Texture2D*>(TextureCubeRHI->GetNativeResource());
			}
			else 
#endif
			{
				FTexture2DRHIRef RenderTargetTextureRHINew;
				TRefCountPtr<FRHITexture2D> Texture2DRHINew;


				RHICreateTargetableShaderResource2D(
					size.X,
					size.Y,
                    Format,
					1,
					TexCreate_Dynamic | TexCreate_Shared | TexCreate_SRGB, // | (Owner.SRGB ? TexCreate_SRGB : 0) ,
					TexCreate_RenderTargetable,
					false,
					CreateInfo,
					RenderTargetTextureRHINew,
					Texture2DRHINew
				);
				Texture.TextureRHI = (FTextureRHIRef&)Texture2DRHINew;
				Texture.SizeX = size.X;
				FRHITexture2D* Texture2DRHI = Texture.TextureRHI->GetTexture2D();
				pTextureUE = static_cast<ID3D11Texture2D*>(Texture2DRHI->GetNativeResource());
			}
			// Open in AMF
			TRefCountPtr<IDXGIResource>	DxgiResource;
			pTextureUE->QueryInterface(IID_PPV_ARGS(DxgiResource.GetInitReference()));
			check(DxgiResource != nullptr);

			HANDLE SharedHandle = nullptr;
			DxgiResource->GetSharedHandle(&SharedHandle);
			check(SharedHandle != nullptr);

			TRefCountPtr<ID3D11Texture2D> SharedResource;
			AmfDevice->OpenSharedResource(SharedHandle, IID_PPV_ARGS(SharedResource.GetInitReference()));
			check(SharedResource != nullptr);
			Texture.TextureAMF = SharedResource;
			AMFTraceWarning(L"FAmfStitchVideoUpdater", L"AMF Surface allocated");
		}
		else
		{
			FreeList.Dequeue(Texture);
		}
		ContextAMF->CreateSurfaceFromDX11Native(Texture.TextureAMF, ppSurface, NULL);
		AllocatedList.AddHead(Texture);
	}
	return AMF_OK;
}

#endif
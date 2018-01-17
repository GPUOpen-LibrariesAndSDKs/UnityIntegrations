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

#pragma once

#include <d3d11.h>
#include <assert.h>
#include <atlbase.h>

#include <memory>
#include <sstream>

#include "PlaybackPipeline.h"

// --------------------------------------------------------------------------
// UnityAVPipeline class. This class marshalls the data from
// the Unity UI into commands that are sent to the AMF AV pipeline.
// This class sets up a DX11 pipeline.
//

class  UnityAVPipeline
{
public:
	static UnityAVPipeline* GetPipeline(int id);

	static void CreatePipeline(int id);

	static void DestroyPipeline(int id);

	static void SetUnityDevice(void *device);

	static void* GetUnityDevice();

	// 

	UnityAVPipeline();

	~UnityAVPipeline();

	void PipelineExecute(const wchar_t* cmd);

	void  PipelineFillAudio(float audioOut[], int bufferLength);

	float PipelineQuery(const wchar_t* type);

	void SetDim(unsigned width, unsigned height);

	void SetTexture(void *texture);

	void CopyResource();

	void SetAmbisonic(bool isAmbisonic);
	void SetAmbisonicAngles(float theta, float phi, float rho);

	bool pipelineRunning();

	bool initialized() const;

	bool paused() const;

	void MutePipeline(bool isMuted);

	void SetPluginPath(const wchar_t* path);

private:

	void UpdateDLLSearchPaths();

	void SetFilename(const wchar_t* urlFileName);

	const std::wstring& Filename() const;

	void SetAMFDevice();

	// Unimplemented
	UnityAVPipeline(const UnityAVPipeline& rhs);

private:
	std::wstring					m_fileName;
	bool							m_initializedPipeline;
	static CComQIPtr<ID3D11DeviceContext>	m_ucontext;	// Unity
	static CComQIPtr<ID3D11Device>			m_udevice;	// Unity
	CComQIPtr<ID3D11DeviceContext>	m_context;	// AMF
	CComQIPtr<ID3D11Device>			m_device;	// AMF
	CComQIPtr<ID3D11Texture2D>		m_texture;
	unsigned						m_width;
	unsigned						m_height;
	std::unique_ptr<PlaybackPipeline> m_pipeline;
	HANDLE							m_sharedHandle;

	bool							m_isPaused;
	bool							m_ambiAudio;
	std::wstring					m_pluginPath;
};

// --------------------------------------------------------------------------
// Defines
#ifdef NDEBUG
#define DBSTREAMOUT( s )
#else
#define DBSTREAMOUT( s )				\
		{									\
		std::wstringstream os_;			\
		os_ << "VideoPlayerAMF ";		\
		os_ << s;						\
		OutputDebugString( os_.str().c_str() );  \
		}
#endif

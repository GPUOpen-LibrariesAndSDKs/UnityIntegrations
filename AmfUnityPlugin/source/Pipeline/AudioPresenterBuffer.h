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

#include "public/samples/CPPSamples/common/AudioPresenter.h"
#include "public/common/AMFSTL.h"

enum AMF_AUDIO_PLAYBACK_STATUS
{
	AMFAPS_UNKNOWN_STATUS = -1,
	AMFAPS_PLAYING_STATUS = 2,
	AMFAPS_PAUSED_STATUS = 3,
	AMFAPS_STOPPED_STATUS = 6,
	AMFAPS_STOPPING_STATUS = 7,
	AMFAPS_EOF_STATUS = 8,
};

class AudioPresenterBuffer: public AudioPresenter
{
public:
	AudioPresenterBuffer();
	virtual ~AudioPresenterBuffer();

	// PipelineElement
	virtual AMF_RESULT SubmitInput(amf::AMFData* pData);
	virtual AMF_RESULT Flush();

	virtual AMF_RESULT  Init();
	virtual AMF_RESULT  Seek(amf_pts pts);

	virtual AMF_RESULT GetDescription(
		amf_int64 &streamBitRate,
		amf_int64 &streamSampleRate,
		amf_int64 &streamChannels,
		amf_int64 &streamFormat,
		amf_int64 &streamLayout,
		amf_int64 &streamBlockAlign
		) const;

	virtual AMF_RESULT Resume(amf_pts currentTime);
	virtual AMF_RESULT Pause();

	void Lock();
	void Unlock();
	void StoreNewAudio(float* audioOut, int outSize);


	unsigned GetSampleRate() const { return 48000; }
	unsigned GetSampleFormat() const { return amf::AMFAF_FLT; }
	unsigned GetBitsPerSample() const { return sizeof(amf_float) * 8; }
	unsigned GetChannelLayout() const { return 3; } //Stereo
	unsigned GetChannels() const { return 2; }

private:

	amf::AMFAudioBufferPtr		m_audioBuffer;
	amf::AMFCriticalSection		m_sect;
	amf::AMFPreciseWaiter		m_Waiter;

	bool		m_isSeeking;
	amf_pts		m_seekGoal;

	// Audio data for unity
	std::queue<amf::AMFAudioBufferPtr>	m_audioBufferQueue;
	size_t								m_audioPosition;
};

typedef std::shared_ptr<AudioPresenterBuffer> AudioPresenterBufferPtr;

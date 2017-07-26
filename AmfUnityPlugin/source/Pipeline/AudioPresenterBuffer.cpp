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

#include "AudioPresenterBuffer.h"

using namespace amf;

#define MAX_BUFFER_LEAD 10 // seconds
#define MAX_QUEUE_LENGTH 3 // each element in the queue is ~0.23ms real time

//-------------------------------------------------------------------------------------------------
AudioPresenterBuffer::AudioPresenterBuffer()
	: m_audioPosition(0)
	, m_isSeeking(false)
{
}

//-------------------------------------------------------------------------------------------------
AudioPresenterBuffer::~AudioPresenterBuffer()
{
}

//-------------------------------------------------------------------------------------------------
AMF_RESULT AudioPresenterBuffer::SubmitInput(amf::AMFData* pData)
{
	amf::AMFLock lock(&m_cs);

	AMF_RESULT err = AMF_OK;

	// hold on to audio buffer for unity
	amf::AMF_DATA_TYPE dataType = pData->GetDataType();
	amf_size dataSize;

	if (dataType == AMF_DATA_AUDIO_BUFFER)
	{
		dataSize = static_cast<AMFAudioBuffer*>(pData)->GetSize();
	}
	else if (dataType == AMF_DATA_BUFFER)
	{
		dataSize = static_cast<AMFBuffer*>(pData)->GetSize();
	}

	if (dataSize > 0)
	{
		m_audioBuffer = static_cast<AMFAudioBuffer*>(pData);
		amf_pts currentPts = m_audioBuffer->GetPts();

		// Store audio data as seperate channels

		// Prevent amf from getting too far ahead
		while (m_audioBufferQueue.size() >= MAX_QUEUE_LENGTH)
		{
			// if queue is maxed out wait approx. length of one buffer
			amf_pts sleepTime = 0;
			sleepTime = m_audioBufferQueue.front()->GetDuration();
			m_Waiter.Wait(sleepTime);
		}
		Lock();
		m_audioBufferQueue.push(m_audioBuffer);
		Unlock();

		if (m_isSeeking)
		{
			Seek(m_seekGoal);
		}
	}

	return err;
}

//-------------------------------------------------------------------------------------------------
AMF_RESULT AudioPresenterBuffer::Flush()
{
	AMF_RESULT err = AMF_OK;
	return err;
}

//-------------------------------------------------------------------------------------------------
AMF_RESULT AudioPresenterBuffer::Init()
{
	AMF_RESULT err = AMF_OK;

	return err;
}

//-------------------------------------------------------------------------------------------------
AMF_RESULT AudioPresenterBuffer::Pause()
{
	AMF_RESULT err = AMF_OK;
	return err;
}

//-------------------------------------------------------------------------------------------------
AMF_RESULT AudioPresenterBuffer::Resume(amf_pts currentTime)
{
	AMF_RESULT err = AMF_OK;

	err = Seek(currentTime);

	return err;
}

//-------------------------------------------------------------------------------------------------
AMF_RESULT AudioPresenterBuffer::Seek(amf_pts pts)
{
	m_seekGoal = pts;
	if (m_audioBufferQueue.empty())
	{
		m_audioPosition = 0;
		return AMF_OK;
	}
	Lock();
	if (pts < m_audioBufferQueue.front()->GetPts() || pts > m_audioBufferQueue.back()->GetPts() + m_audioBufferQueue.back()->GetDuration())
	{  // before oldest or after most recent, clear out the queue
		m_audioPosition = 0;
		m_isSeeking = true;
		while (!m_audioBufferQueue.empty())
		{
			m_audioBufferQueue.pop();
		}
	}
	else
	{ 
		while (!m_audioBufferQueue.empty())
		{
			if (pts > m_audioBufferQueue.front()->GetPts() + m_audioBufferQueue.front()->GetDuration())
			{
				m_audioBufferQueue.pop();
			}
			else
			{
				m_audioPosition = ((pts - m_audioBufferQueue.front()->GetPts()) / AMF_SECOND) * GetSampleRate() * GetChannels();
				break;
			}
		}
		m_isSeeking = false;
	}
	Unlock();
	return AMF_OK;
}

//-------------------------------------------------------------------------------------------------
AMF_RESULT AudioPresenterBuffer::GetDescription(
	amf_int64 &streamBitRate,
	amf_int64 &streamSampleRate,
	amf_int64 &streamChannels,
	amf_int64 &streamFormat,
	amf_int64 &streamLayout,
	amf_int64 &streamBlockAlign
	) const
{
	streamBitRate = GetBitsPerSample() * GetChannels() * GetSampleRate();
	streamSampleRate = GetSampleRate();
	streamChannels = GetChannels();
	streamFormat = GetSampleFormat();
	streamLayout = GetChannelLayout();
	streamBlockAlign = GetBitsPerSample() * GetChannels() / 8;

	return AMF_OK;
}

//-------------------------------------------------------------------------------------------------
void AudioPresenterBuffer::Lock(){
	m_sect.Lock();
}

//-------------------------------------------------------------------------------------------------
void AudioPresenterBuffer::Unlock(){
	m_sect.Unlock();
}

//-------------------------------------------------------------------------------------------------
void AudioPresenterBuffer::StoreNewAudio(float* audioOut, int outSize)
{
	if (m_audioBufferQueue.empty())
	{
		return;
	}
	Lock();
	size_t remainingSamples = m_audioBufferQueue.front()->GetSampleCount() * m_audioBufferQueue.front()->GetChannelCount() - m_audioPosition;

	// copy data
	while (outSize > remainingSamples)
	{
		// copy what's left in current buffer
		memcpy(audioOut, (float*)m_audioBufferQueue.front()->GetNative() + m_audioPosition, remainingSamples * sizeof(float));

		// prep for next buffer
		audioOut += remainingSamples;
		outSize = outSize - (int) remainingSamples;
		m_audioPosition = 0;

		m_audioBufferQueue.pop();
		if (m_audioBufferQueue.empty())
		{
			return;
		}
		remainingSamples = m_audioBufferQueue.front()->GetSampleCount() * m_audioBufferQueue.front()->GetChannelCount();
		
	}

	// copy what's needed from new buffer
	memcpy(audioOut, (float*)m_audioBufferQueue.front()->GetNative() + m_audioPosition, outSize * sizeof(float));

	m_audioPosition += outSize;
	Unlock();
}
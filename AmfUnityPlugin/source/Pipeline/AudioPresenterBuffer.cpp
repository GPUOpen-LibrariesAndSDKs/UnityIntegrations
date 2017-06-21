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

#define MAX_BUFFER_LEAD 100000 // 10 seconds

//-------------------------------------------------------------------------------------------------
AudioPresenterBuffer::AudioPresenterBuffer()
	: m_lastBufferPts(-1),
	m_audioPosition(0),
	m_startPts(-1)
{
}

//-------------------------------------------------------------------------------------------------
AudioPresenterBuffer::~AudioPresenterBuffer()
{
	m_audioData.clear();
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

		// if first buffer
		if (m_startPts <= 0)
		{
			m_startPts = currentPts;
		}

		if (m_lastBufferPts < currentPts)
		{

			amf_int32 numSamples = m_audioBuffer->GetSampleCount();
			amf_int32 sampleSize = m_audioBuffer->GetSampleSize();
			amf_int32 dataSize = numSamples * sampleSize * GetChannels(); //in bytes
			
			if (m_audioData.capacity() < m_audioData.size() + numSamples * GetChannels())
			{
				Lock();
				m_audioData.reserve(m_audioData.size() + GetSampleRate() * GetChannels() * (MAX_BUFFER_LEAD / 10000)); //reserve ten seconds of samples
				Unlock();
				m_Waiter.Wait(MAX_BUFFER_LEAD / 2);
			}

			float* bufferData = (float*)m_audioBuffer->GetNative();
			Lock();
			m_audioData.insert(m_audioData.end(), bufferData, bufferData + numSamples * GetChannels());
			Unlock();

			m_lastBufferPts = currentPts;

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

	if (pts < m_startPts) // pts is before clip starts
	{
		m_audioPosition = 0;
	}
	else if (pts > m_lastBufferPts) //pts is later than current buffer
	{
		m_audioPosition = m_audioData.size();
	}
	else
	{
		//in scope, calc new m_audioPosition
		m_audioPosition = static_cast<size_t>((pts - m_startPts) * GetChannels() * GetSampleRate() / AMF_SECOND);
	}

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
	if (!m_audioData.empty())
	{
		if (m_audioPosition + outSize > m_audioData.size())
		{
			return;
		}
		Lock();
		memcpy(audioOut, m_audioData.data() + m_audioPosition, outSize * sizeof(float));
		Unlock();
		m_audioPosition += outSize;
	}
}

/*-------------------------------------------------------------------------
 File    : $Archive: $
 Author  : $Author: $
 Version : $Revision: $
 Orginal : 2005-xx-xx, 15:50
 Descr   : Channel and Mixer handling for XM playback
 
 
 Modified: $Date: $ by $Author: $
 ---------------------------------------------------------------------------
 TODO: [ -:Not done, +:In progress, !:Completed]
 <pre>
  - Proper streo handling (channels write them selves properly but the mixer forces everything down to Mono)
 </pre>
 
 
 \History
  - 10.05.09, FKling, Ported to Mac and wrote RingBuffer handling for output
 ---------------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include <vector>
#include "mutex.h"
#include "ringbuffer.h"
#include "mixer.h"

using namespace Goat;
using namespace std;

int glb_Verbose = 0;	// Lousy, used by both filexm.cpp and mixer

#ifdef _DEBUG
#define dprintf (glb_Verbose)printf
#else
#define dprintf if(glb_Verbose)printf
#endif

//////////////////////////////////////////////////////////////////////////
//
// stream, should be moved to own file
//
Stream::Stream()
{
	ptrMixer = NULL;
	isStreamActive = false;
}
Stream::~Stream()
{
	// ptrMixer->removeStream();
}
void Stream::setMixer(Mixer *_ptrMixer)
{
	ptrMixer = _ptrMixer;
}
bool Stream::render(unsigned int dwNumSamples)
{
	return true;
}
bool Stream::getStreamActive()
{
	return isStreamActive;
}
void Stream::setStreamActive(bool activeate /* = true */)
{
	isStreamActive = activeate;
}

//////////////////////////////////////////////////////////////////////////
//
// stream file, should be moved to own file
//
StreamFile::StreamFile()
{

}
StreamFile::~StreamFile()
{

}
bool StreamFile::load(char *filename)
{
	return false;
}
bool StreamFile::render(unsigned int dwNumSamples)
{
	return false;
}

//////////////////////////////////////////////////////////////////////////
Channel::Channel()
{
	lMul = 1.0;
	rMul = 1.0;
	ptrByteStream = NULL;
	ptrShortStream = NULL;
	hasDataStream = false;
	sampleLength = 0;
	currentPos   = 0;
	isActive = false;
	isMuted = false;
	freq = 100;
	loop = loopActive = false;
	volume = 1.0f;
	mDataFormat = fUnknown;
}
Channel::~Channel()
{

}
//////////////////////////////////////////////////////////////////////////
//
// TODO: replace "useLoop" by enum, and support for different loop styles
//
void Channel::setLoopParameters(bool useLoop, unsigned int offset, unsigned int length)
{
	loop = useLoop;
	loopActive = false;
	dwLoopOffset = offset;
	dwLoopLength = length;
}

float Channel::getVolume(void)
{
	return volume;
}
void Channel::setVolume(float vol)
{
	volume = vol;
}

void Channel::reset(bool use /* = true */)
{	
	currentPos = 0;
	isActive = use;
	loopActive = false;
}
void Channel::setFreq(float _freq)
{
	freq = _freq;
	if (freq < 100)
		freq = 100;

}
void Channel::setPositionOffset(unsigned int offset)
{
	currentPos = offset;
}
void Channel::resetWritePosition()
{
	dwWritePos = 0;
}
bool Channel::getMute(void)
{
	return isMuted;
}

void Channel::mute(bool mute /* = true */)
{
	isMuted = mute;
}


void Channel::setDataStream(void *_ptrStream, unsigned int _dwSamples, eDataFormat mFormat)
{
	sampleLength  = _dwSamples;
	currentPos    = 0;
	isActive      = true;	
	mDataFormat   = mFormat;

	switch (mDataFormat)
	{
	case f8BitSigned :
		ptrByteStream = (char *)_ptrStream;
		break;
	case f16BitSigned :
		ptrShortStream = (signed short *)_ptrStream;
		break;
	case f8BitUnsigned :
	case f16BitUnsigned :
	case f32BitSigned :
	case f32BitUnsigned :
	case fFloatBuffer :
	case fDoubleBuffer :
		printf ("[Warning] channel-sample format not supported!");
		return;
		break;
	}

	hasDataStream = true;

}
void Channel::setStreamOffset(unsigned int dwSampleOffset)
{
	currentPos = dwSampleOffset;
}
//////////////////////////////////////////////////////////////////////////
//
// nice fast and stupid renderer of samples
//
void Channel::render(Mixer *ptrMixer, int dwNumSamples)
{
	float v1,v2;
	float *ptrLeft, *ptrRight,val,b_mul;
	float delta;
	float pos = 0;
	float pa,pb;
	unsigned int ca,cb;
	float mulActiveChannel;
	unsigned char *ptrUStream = (unsigned char *)ptrByteStream;

	if (isMuted)
		return;

	if (!isActive)
		return;

	if (!hasDataStream)
		return;

	mulActiveChannel = 1.0f / (float)ptrMixer->getChannelCount();

	//
	// end of sample already
	//
	if (currentPos >= sampleLength)
		return;


	//
	// 
	//
	delta = freq / (44100.0f);


	ptrLeft  = ptrMixer->getLeftChannelBuffer();
	ptrRight = ptrMixer->getRightChannelBuffer();

	switch (mDataFormat)
	{
	case f8BitSigned :
	case f8BitUnsigned :
		b_mul = (1.0f / 128.0f);
		break;
	case f16BitSigned :
	case f16BitUnsigned :
		b_mul = (1.0f / 32768.0f);
		break;
	default:
		b_mul = 1.0f;
	}
	pos = (float)currentPos;
//	printf ("UseLoop: %s\n",loop?"yes":"no");
//	printf ("Pos: %f\n",pos);

	int nSamples = dwNumSamples;
	while(nSamples)
	{
		//
		// loop construction was a little flunky
		// still dont know why I did this, but I recall something about
		// "first round, play whole sample, then start loop stuff.." but
		// that sounds quite silly..
		//
		// TODO: add support for ping-pong loop styles...
		//
		if (loop & loopActive)
		{
			if (pos >= (dwLoopLength + dwLoopOffset))
				pos = (float)dwLoopOffset;

		} else
		{
			if ((loop) & (pos >= (dwLoopLength + dwLoopOffset)))
			{
				pos = (float)dwLoopOffset;
				loopActive = true;
			}

		}

		//
		// alias, TODO: fix better code, bi-cubic for instance..
		//
		pa = pos-delta;
		pb = pos+delta;
		ca = (unsigned int)(pa < 0 ? 0:pa);
		cb = (unsigned int)(pb > sampleLength ? pos : pb);

		// 
		// alias values
		//
		switch (mDataFormat)
		{
		case f8BitSigned:
			v1 = volume * (b_mul * (float)ptrByteStream[ca]);
			v2 = volume * (b_mul * (float)ptrByteStream[cb]);
			val = volume * (b_mul * (float)ptrByteStream[currentPos]);
			break;
		case f16BitSigned:
			v1 = volume * (b_mul * (float)ptrShortStream[ca]);
			v2 = volume * (b_mul * (float)ptrShortStream[cb]);
			val = volume * (b_mul * (float)ptrShortStream[currentPos]);
			break;
		}

		//
		// alias
		// 
		val = 0.2f*v1 + 0.2f*v2 + 0.6f*val;
//		val = val>1.0?1.0:val;
//		val = val<-1.0?-1.0:val;

		//
		// --> divide by number of channels
		// [gnilk, 2023-11-24] - we should never do this - this is now how audio works - but well - this is history...
		// val = val * mulActiveChannel;


		*ptrLeft  += val;
		*ptrRight += val;

		ptrLeft++;
		ptrRight++;

		pos += delta;
		currentPos = (int)pos;

		nSamples--;
	}
}



//////////////////////////////////////////////////////////////////////////

Mixer::Mixer()
{
	dwChannelCount = 0;
	leftChannel = rightChannel = NULL;
}

Mixer::~Mixer()
{

}

bool Mixer::initialize()
{
	dsLastWritePos = 0;

	dwChannelSize = MIXER_BUFFER_SIZE;
	leftChannel = new float[MIXER_BUFFER_SIZE];
	rightChannel = new float[MIXER_BUFFER_SIZE];

	// This is the mixed output of the assigened streams
	// Only mono for now, sorry..
	pMonoOutput = new RingBuffer();


	mixerFirstTime = true;

	setBPM(125);

	//dwChannelSize = 400;
	printf ("[INFO] Init ok...\n");

	return true;
}

bool Mixer::close()
{
	if (leftChannel) delete []leftChannel;
	if (rightChannel) delete []rightChannel;

	return true;
}

void Mixer::setBPM(int bpm)
{
	// Calculate how man samples each channel shall have
	dwChannelSize = 60*44100 / ( 32 * bpm );
}

void Mixer::addStream(Stream *ptrStream, bool isActive /* = true */)
{
	ptrStream->setMixer(this);
	ptrStream->setStreamActive(isActive);

	listStreams.push_back(ptrStream);
}
float *Mixer::getLeftChannelBuffer(void)
{
	return leftChannel;
}
float *Mixer::getRightChannelBuffer(void)
{
	return rightChannel;
}
unsigned int Mixer::getChannelBufferSize(void)
{
	return dwChannelSize;
}
void Mixer::setChannelBuffersize(unsigned int dwBS)
{
	dprintf("Mixer, new channel buffer size: %d bytes\n",dwBS);
	dwChannelSize = dwBS;
}
void Mixer::addStreamChannel(unsigned int addCount)
{
	dwChannelCount+=addCount;
}
void Mixer::delStreamChannel(unsigned int delCount)
{
	int overFlow;

	overFlow = dwChannelCount;
	overFlow -= delCount;

	if (overFlow < 0)
		dwChannelCount = 0;
	else
		dwChannelCount-=delCount;
	
}
unsigned int Mixer::getChannelCount()
{
	return dwChannelCount;
}

// TODO: this should move to another place
void Mixer::renderStreams()
{
	int i;
	for (i=0;i<dwChannelSize;i++)
		leftChannel[i] = rightChannel[i] = 0;
	
	for (i=0;i<listStreams.size();i++)
	{
		// only render active streams
		if (listStreams[i]->getStreamActive())
			listStreams[i]->render(dwChannelSize);
	}
	// Auto flush
	flush(dwChannelSize);
}

void Mixer::flush(int nSamples)
{
	if (pMonoOutput != NULL);
	{
		pMonoOutput->Write(leftChannel, 0, sizeof(float) * nSamples);
	}
}

#define merrx(x) { printf("FAILED: %s\n",x); exit(1); }


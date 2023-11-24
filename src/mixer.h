/*-------------------------------------------------------------------------
 File    : $Archive: $
 Author  : $Author: $
 Version : $Revision: $
 Orginal : 2005-xx-xx, 15:50
 Descr   : Mixer and Channel definitions
 
 
 Modified: $Date: $ by $Author: $
 ---------------------------------------------------------------------------
 TODO: [ -:Not done, +:In progress, !:Completed]
 <pre>
 </pre>
 
 
 \History
 - 10.05.09, FKling, Ported to Mac and wrote RingBuffer handling for output
 ---------------------------------------------------------------------------*/

#ifndef __FKLING_MIXER_H__
#define __FKLING_MIXER_H__


#include <math.h>
#include <vector>
#include "mutex.h"
#include "ringbuffer.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

#define TIMER_CALL_INTERVAL 10
#define MIXER_BUFFER_SIZE 60*44100
#define MIXER_MIX_FREQ 44100

using namespace Goat;

class Mixer;

class Stream
{
protected:
	Mixer *ptrMixer;		// rewrite this...
	bool isStreamActive;
public:
	Stream();
	virtual ~Stream();
	void setMixer(Mixer *ptrMixer);
	virtual bool render(unsigned int dwNumSamples);
	virtual bool getStreamActive();
	virtual void setStreamActive(bool activeate = true);
};

class StreamFile : public Stream
{
public:
	StreamFile();
	virtual ~StreamFile();
	virtual bool load(char *filename);
	virtual bool render(unsigned int dwNumSamples);
};

class Channel
{
public:
	enum eDataFormat
	{
		fUnknown,
		f8BitSigned,
		f8BitUnsigned,
		f16BitSigned,
		f16BitUnsigned,
		f32BitSigned,
		f32BitUnsigned,
		fFloatBuffer,
		fDoubleBuffer,
	};

protected:
	float lMul, rMul;	// panning multipliers
	char *ptrByteStream;
	signed short *ptrShortStream;
	unsigned int sampleLength;
	unsigned currentPos;
	bool isActive,hasDataStream,isMuted;
	float freq;
	bool loop,loopActive;
	unsigned int dwLoopOffset;
	unsigned int dwLoopLength;
	float volume;
	
	unsigned int dwWritePos;
	eDataFormat mDataFormat;
public:
	float panning;
	
public:
	Channel();
	virtual ~Channel();

	void setPositionOffset(unsigned int offset);
	void setFreq(float freq);
	void resetWritePosition();
	void reset(bool use = true);
	void setDataStream(void *_ptrStream, unsigned int _dwSamples, eDataFormat mFormat);
	void setStreamOffset(unsigned int dwSampleOffset);
	void render(Mixer *ptrMixer, int dwNumSamples);
	void setLoopParameters(bool useLoop, unsigned int offset, unsigned int length);
	void setVolume(float vol);
	float getVolume(void);
	void mute(bool mute = true);
	bool getMute(void);

	bool __inline getActive() { return isActive; };
	void __inline setActive(bool bActive) { isActive = bActive; };
};


class Mixer
{
private:
	unsigned int dsBufferSize,dsLastWritePos;

	unsigned int dwChannelSize;
	float *leftChannel,*rightChannel;

	bool mixerFirstTime;
	unsigned int dwChannelCount;

	std::vector <Stream *> listStreams;
public:	
	RingBuffer *pMonoOutput;

public:
	Mixer();
	virtual ~Mixer();
	bool initialize();
	bool close();

	void setBPM(int bpm);
	void flush(int nSamples);	
	void renderStreams();

//////////////////////////////////////////////////////////////////////////
	void addStream(Stream *ptrStream, bool isActive = true);
	void addStreamChannel(unsigned int addCount);
	void delStreamChannel(unsigned int delCount);
	unsigned int getChannelCount();
	void setChannelBuffersize(unsigned int dwBS);
	float *getLeftChannelBuffer(void);
	float *getRightChannelBuffer(void);
	unsigned int getChannelBufferSize(void);
};

#endif
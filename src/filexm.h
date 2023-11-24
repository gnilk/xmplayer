#ifndef __FKLING_FILE_XM_H__
#define __FKLING_FILE_XM_H__

#include "mixer.h"


//////////////////////////////////////////////////////////////////////////
//
// structure definitions are old...
// I did not verify them when I imported them into this project...
//

#pragma pack( push,1)

typedef struct XM_MODHEAD XM_MODHEAD;
typedef struct XM_PATTERN XM_PATTERN;
typedef struct XM_MODHEAD XM_MODHEAD;
typedef struct XM_NOTE XM_NOTE;
typedef struct XM_SAMPLE XM_SAMPLE;
typedef struct XM_INSTRUMENT XM_INSTRUMENT;
typedef struct XM_INSTRUMENT_EXTRA XM_INSTRUMENT_EXTRA;

struct XM_MODHEAD
{
	char id[17];			// 'Extended Module: '
	char name[20];
	unsigned char x;						// $1a
	char tracker[20];
	unsigned short version;			// $104 - only supported version
	unsigned int headersize;
	unsigned short songlen;
	unsigned short restartpos;
	unsigned short channels;
	unsigned short patterns;
	unsigned short instruments;
	unsigned short freqtable;
	unsigned short deftempo;
	unsigned short defbpm;
	unsigned char ordertable[256];
};
struct XM_NOTE
{
	unsigned char note;
	unsigned char instr;
	unsigned char volume;
	unsigned char effect;
	unsigned char effectdata;
};
struct XM_PATTERN
{
	unsigned int headersize;
	unsigned char packingtype;  // should always be 0
	unsigned short rows;
	unsigned short packedpatternsize;
	XM_NOTE *note_data;
};
struct XM_SAMPLE
{
	unsigned int len;
	unsigned int loopstart;
	unsigned int looplen;
	unsigned char vol;
	char finetune;
	unsigned char looptype;   //Bit 0-1: 0 = No loop, 1 = Forward loop, 2 = Ping-pong loop 4 = 16-bit sampledata
	unsigned char panning;
	char relativenote;
	unsigned char reserved;
	char	name[22];
	signed short *data;
};
struct XM_INSTRUMENT
{
	unsigned int hsize;
	char name[22];
	unsigned char type; // should always be zero, but isnt...
	unsigned short samples;
	XM_INSTRUMENT_EXTRA *instrextra;
	XM_SAMPLE *sample;
	/*
	<< "Instrument Size" field tends to be more than the
	actual size of the structure documented here (it
	includes also the extended instrument sample header
	above). So remember to check it and skip the additional
	bytes before the first sample header >>
	*/
};
struct XM_INSTRUMENT_EXTRA
{
	// if samples > 0 then this will be present in the file, otherwise not...
	unsigned int	smphsize;
	unsigned char	votesmpnumber[96];
	unsigned short volenvpoints[12*2];
	unsigned char	panenvpoints[48];
	unsigned char	numvolenvpoints;
	unsigned char	numpanenvpoints;
	unsigned char	volsustainpoint; // all these are indexes to respective array!
	unsigned char	volloopstart;
	unsigned char	volloopend;
	unsigned char	pansystainpoint;
	unsigned char	panloopstart;
	unsigned char	panloopend;
	unsigned char	voltype; //bit 0: On; 1: Sustain; 2: Loop
	unsigned char	pantype; //bit 0: On; 1: Sustain; 2: Loop
	unsigned char	vibrato;
	unsigned char	vibratotype;
	unsigned char	vibratosweep;
	unsigned char	vibratodepth;
	unsigned char	vibratorate;
	unsigned short	volfadeout;
	unsigned short	reserved;

};

#pragma pack( pop)


#define XM_FREQ_LINEAR		0x01

#define XM_ENVELOPE_ACTIVE	0x01
#define XM_ENVELOPE_SUSTAIN	0x02
#define XM_ENVELOPE_LOOP	0x04

#define XM_KEYOFF 97

//////////////////////////////////////////////////////////////////////////
//
// when an effect is active, these values are or:ed into the "chControl"
// variable of the XMChannelEffect structure, they are then used to 
// send commands down to the mixer...
//

#define XM_CH_NONE			0x000
#define XM_CH_VOLUME_FX		0x001
#define XM_CH_SET_VOL		0x002
#define XM_CH_VIBRATO		0x004
#define XM_CH_FREQ			0x008
#define XM_CH_VOL_SLIDE		0x010
#define XM_CH_PORTAMENTO	0x020
#define XM_CH_NOTE_DELAY	0x040
#define XM_CH_SLIDE		0x080

//////////////////////////////////////////////////////////////////////////
//
// intermediate structure, I use it to control each channel internally
// since I need to keep track of states and last-command issued and
// other stuff...  referenced normally as "fxNote[x]"
//
struct XMChannelEffect
{
	unsigned int chControl;

	bool	keyoff;

	int	vol;
	int	fadeoutvol;

	int	voldelta;
	int	volSlideCmdLastUsed;
	//
	// vibrato releated
	//
	int	vibpos;
	int	vibdepth;
	int	vibspeed;
	int	vibwave;
	//
	// portamento handling
	//
	int	portatarget;
	int	portaspeed;

	//
	// placeholders for frequency information
	//
	int	period;
	int	freq;
	int	freqdelta;
	//
	// Envelope handling
	//
	int	envvol;
	int	envvolpos;
	int	envvoltick;
	bool	envvolstopped;
	float	envvoldelta;
	float	envvolipol;

	//
	// place holder for others
	//
	int	tickDelayForNote;
	

	struct XM_NOTE note;
	struct XM_SAMPLE *ptrSample;
	struct XM_INSTRUMENT_EXTRA *ptrInstrExtra;
	Channel outChannel;
};

class XMFile : public StreamFile
{
private:
	struct XM_MODHEAD xmHeader;
	struct XM_PATTERN *ptrPatterns;
	struct XM_INSTRUMENT *ptrInstruments;

	bool readInstrumentData(FILE *f, XM_INSTRUMENT *ptrInstrument,unsigned int fPointStart);
	bool decodePattern(XM_PATTERN *ptrPattern, unsigned char *ptrData);

	void resetXMChannel(int ch);
	void resetAllNotes();

	void doTick();
	void updateEffects();
	float getFrequency(unsigned int note, int fineTune);
	void setChannelVolume(int ch);

	void slideVolumeUp(int ch, int count);
	void slideVolumeDown(int ch, int count);
	void slideVolume(int ch);
	void processPortamento(int ch);
	void processSliding(int ch);
	void processVolumeFx(int ch, unsigned char volume);
	void processVibrato(int ch);
	void processEnvelope(int ch);

	void printPatternRow(XM_PATTERN *ptrPattern, unsigned int row);

	unsigned int dwTempo;	
	unsigned int dwCurrentTick;
	unsigned int dwSamplesPerTick;
	int   iSamplesLeft;
	unsigned int dwPatternDelay;

	unsigned int dwCurrentOrder;
	unsigned int dwCurrentPattern;
	unsigned int dwCurrentRow;
	bool isPlaying;
	bool hasFile;

	int glbVolume;

	//
	// we use how many we may like!
	//
	struct XMChannelEffect fxNote[256];
public:
	XMFile();
	virtual ~XMFile();
	virtual bool load(char *filename);
	virtual bool render(unsigned int dwNumSamples);

	virtual void setCurrentOrder(unsigned int newOrder);
	virtual bool getPlayInfo(unsigned int *pOrder, unsigned int *pPattern, unsigned int *pPatPos);

	void reset();
	void setSpeed(int newSpeed);

	void muteAll();
	void enableChannel(int ch);
	void disableChannel(int ch);

	void dumpExtraInstrumentData(int instr);

	//
	// we need to trig this
	//
	virtual void setStreamActive(bool activeate = true);
};

#endif
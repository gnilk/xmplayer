//////////////////////////////////////////////////////////////////////////
/*
File        : FileXM.cpp
Author(s)   : Gnilk
Original    : 2004-02-xx
Description : XM Render

	Wrote it because I realized I had not written any module player before..

	Simple XM player, far from all effects implemented!
	Very much based on the work of others...

	Without other implementations I would not stand a chance
	implementing the effects nor the envelope handling

	For instance, where does it say (in the docs that is) that the volume
	envelope is stored interleaved (tick, value) ?!??!

	No panning effects supported...

TODO:
	! Relative Note seems to be a little buggy
	+ Sample loop, must be supported by channel mixer...

	- Following effects have priority
		- 05: Portamento + Volume Slide (simple, just reuse last portamento, and do volume slide)
		+ Ex: Extended effects
		+ Instrument Envelope handling
		+ Key-Off handling

IMPLEMENTED:
		:01 - Portamento Up
		:02 - Portamento Down
		:03 - Portamento, slide to note
		:04 - Vibrato
		:06 - Continue Vibrato but also slide volume
		:09 - Sample offset
		:0a - Volume slide
		:0c - Set Volume
		:0d - Pattern break
		:0e - Extended effect
		:0d - note delay
		:0f - Set speed
		:10 - Global volume (doesnt work though  =)

Changes: 
-- Date -- | -- Name ------- | -- Did what...                              
2006-02-19 | Gnilk           | Note delay command implemented
           |                 | Wrote some commentes to the source...


*/
#include <assert.h>
#include "filexm.h"

extern int glb_Verbose;
#ifdef _DEBUG
#define dprintf if(glb_Verbose)printf
#else
//#define dprintf sizeof
#define dprintf if(glb_Verbose)printf
#endif



/*
!
!			     Sample data:
!			     ------------
!
!	   ?	     Sample data (signed): The samples are stored
!			     as delta values. To convert to real data:
!
!			     old=0;
!			     for i=1 to len
!				new=sample[i]+old;
!				sample[i]=new;
!				old=new;

A simle packing scheme is also adopted, so that the patterns not become
TOO large: Since the MSB in the note value is never used, if is used for
the compression. If the bit is set, then the other bits are interpreted
as follows:

bit 0 set: Note follows
1 set: Instrument follows
2 set: Volume column byte follows
3 set: Effect follows
4 set: Guess what!


Period = 10*12*16*4 - Note*16*4 - FineTune/2;
Frequency = 8363*2^((6*12*16*4 - Period) / (12*16*4));

*/

static char * glbNoteStrings[]=
{
		"---",
		"C-0","C#0","D-0","D#0","E-0","F-0","F#1","G-0","G#0","A-0","A#1","B-0",
		"C-1","C#1","D-1","D#1","E-1","F-1","F#1","G-1","G#1","A-1","A#1","B-1",
		"C-2","C#2","D-2","D#2","E-2","F-2","F#2","G-2","G#2","A-2","A#2","B-2",
		"C-3","C#3","D-3","D#3","E-3","F-3","F#3","G-3","G#3","A-3","A#3","B-3",
		"C-4","C#4","D-4","D#4","E-4","F-4","F#4","G-4","G#4","A-4","A#4","B-4",
		"C-5","C#5","D-5","D#5","E-5","F-5","F#5","G-5","G#5","A-5","A#5","B-5",
		"C-6","C#6","D-6","D#6","E-6","F-6","F#6","G-6","G#6","A-6","A#6","B-6"
		"C-7","C#7","D-7","D#7","E-7","F-7","F#7","G-7","G#7","A-7","A#7","B-7"
};



XMFile::XMFile()
{
	memset(&xmHeader,0,sizeof(XM_MODHEAD));
	ptrPatterns = NULL;
	ptrInstruments = NULL;
	isPlaying = false;
	hasFile = false;
	dwCurrentOrder = dwCurrentPattern = dwCurrentRow = 0;
	glbVolume = 64;
	dwSamplesPerTick = 800;	// just set to something
	iSamplesLeft = -1;
}

//////////////////////////////////////////////////////////////////////////
//
// default destructor
//
XMFile::~XMFile()
{
	// TODO: dispose of everything
}
//////////////////////////////////////////////////////////////////////////
//
// just dump some data
//
void XMFile::dumpExtraInstrumentData(int instr)
{
	int i;
	XM_INSTRUMENT *ptrInstrument;

	ptrInstrument = &ptrInstruments[instr];
	if (!ptrInstrument->instrextra)
		return;

	printf ("Instrument dump: %d\n",instr);
	printf ("Name...: %.20s\n",ptrInstrument->name);
	printf ("Samples: %d\n",ptrInstrument->samples);

	if (ptrInstrument->instrextra->voltype & XM_ENVELOPE_ACTIVE)
	{
		printf ("    Volume Envelope: %d points\n",ptrInstrument->instrextra->numvolenvpoints);
		if (ptrInstrument->instrextra->voltype & XM_ENVELOPE_SUSTAIN)
			printf ("    Sustain point..: %d\n",ptrInstrument->instrextra->volsustainpoint);

		for (i=0;i<ptrInstrument->instrextra->numvolenvpoints;i++)
		{
			if ((i == ptrInstrument->instrextra->volsustainpoint) && (ptrInstrument->instrextra->voltype & XM_ENVELOPE_SUSTAIN))
				printf ("      S%.2d:  %d:%d\n",i,ptrInstrument->instrextra->volenvpoints[i*2],ptrInstrument->instrextra->volenvpoints[i*2+1]);
			else
				printf ("       %.2d:  %d:%d\n",i,ptrInstrument->instrextra->volenvpoints[i*2],ptrInstrument->instrextra->volenvpoints[i*2+1]);

		}

	}

	printf ("Note Mappings..............\n");
	if (ptrInstrument->samples > 0)
	{
		for (i=0;i<96;i++)
			printf("  %.2x -> %.2d\n",i,ptrInstrument->instrextra->votesmpnumber[i]);
	}

	printf ("Samples................\n");
	for (i=0;i<ptrInstrument->samples;i++)
	{
		printf("Sample: %d\n",i);

		if (ptrInstrument->sample[i].len)
		{
#ifdef _DEBUG
			printf("    %d:Name..: %.22s\n",i,ptrInstrument->sample[i].name);
			printf("    %d:Length: %d\n",i,ptrInstrument->sample[i].len);
			printf("    %d:Type..: %s\n",i,(ptrInstrument->sample[i].looptype & 16) ? "16 bit" : "8 bit");
			printf("    %d:FTune.: %d\n",i,ptrInstrument->sample[i].finetune);
			printf("    %d:Loop..: %s\n",i,(ptrInstrument->sample[i].looptype & 3) ? "Yes" : "No");
			printf("    %d:LStart: %d\n",i,ptrInstrument->sample[i].loopstart);
			printf("    %d:LEnd..: %d\n",i,ptrInstrument->sample[i].looplen);
			printf("    %d:RNote.: %d\n",i,ptrInstrument->sample[i].relativenote);
			printf("\n");
#endif
		}
	}
	printf ("--------------------------------\n");



}

#ifndef WIN32
static bool ReadFile(FILE *f, void *pDest, unsigned int nBytesToRead, unsigned int *outBytesRead, void *pAsync)
{
	unsigned int tmp;
	tmp = fread(pDest, nBytesToRead, 1, f);
	if (outBytesRead != NULL) *outBytesRead = tmp;
	return true;
}
#endif

//////////////////////////////////////////////////////////////////////////
//
// read all instrument data
//
bool XMFile::readInstrumentData(FILE *f, XM_INSTRUMENT *ptrInstrument, unsigned int fPointStart)
{
	unsigned int dwRead;
#ifdef _DEBUG
	printf ("Name...: %.20s\n",ptrInstrument->name);
	printf ("Samples: %d\n",ptrInstrument->samples);
#endif

	// this is checked before calling...
	if (!ptrInstrument->samples)
		return false;

	//
	// 16 is max sample count
	//
	if (ptrInstrument->samples > 16)
		return false;


	//
	// read extra header
	//
	ptrInstrument->instrextra = new struct XM_INSTRUMENT_EXTRA;
	if (!ReadFile(f,ptrInstrument->instrextra,sizeof(struct XM_INSTRUMENT_EXTRA),&dwRead,NULL))
	{
		printf ("[error] Extra data for instrument failed: %d\n",dwRead);
		return false;
	}

	ptrInstrument->instrextra->volfadeout*=2;
#ifdef _DEBUG


	printf ("    VolumeFadeout: %d\n",ptrInstrument->instrextra->volfadeout);

	if (ptrInstrument->instrextra->voltype & XM_ENVELOPE_ACTIVE)
	{
		int i;
		printf ("    Volume Envelope: %d points\n",ptrInstrument->instrextra->numvolenvpoints);
		if (ptrInstrument->instrextra->voltype & XM_ENVELOPE_SUSTAIN)
			printf ("    Sustain point..: %d\n",ptrInstrument->instrextra->volsustainpoint);

		for (i=0;i<ptrInstrument->instrextra->numvolenvpoints;i++)
		{
			if ((i == ptrInstrument->instrextra->volsustainpoint) && (ptrInstrument->instrextra->voltype & XM_ENVELOPE_SUSTAIN))
				printf ("      S%.2d:  %d:%d\n",i,ptrInstrument->instrextra->volenvpoints[i*2],ptrInstrument->instrextra->volenvpoints[i*2+1]);
			else
				printf ("       %.2d:  %d:%d\n",i,ptrInstrument->instrextra->volenvpoints[i*2],ptrInstrument->instrextra->volenvpoints[i*2+1]);

		}

	}
#endif


	fPointStart += ptrInstrument->hsize;
	fseek(f,fPointStart, SEEK_SET);
//	SetFilePointer(hf,fPointStart,NULL,FILE_BEGIN);
	
	unsigned int i,j;
	ptrInstrument->sample = new struct XM_SAMPLE[ptrInstrument->samples];
	for (i=0;i<ptrInstrument->samples;i++)
	{
		
		if (!ReadFile(f,&ptrInstrument->sample[i],sizeof(struct XM_SAMPLE)-sizeof(void *),&dwRead,NULL))
		{
			printf ("[error] Failed while reading sample: %d, read %d bytes\n",i,dwRead);
			return false;
		}
		int apa;
		ptrInstrument->sample[i].relativenote;

		if (ptrInstrument->sample[i].len)
		{
#ifdef _DEBUG
			printf("    %d:Name..: %.22s\n",i,ptrInstrument->sample[i].name);
			printf("    %d:Length: %d\n",i,ptrInstrument->sample[i].len);
			printf("    %d:Type..: %s\n",i,(ptrInstrument->sample[i].looptype & 16) ? "16 bit" : "8 bit");
			printf("    %d:FTune.: %d\n",i,ptrInstrument->sample[i].finetune);
			printf("    %d:Loop..: %s\n",i,(ptrInstrument->sample[i].looptype & 3) ? "Yes" : "No");
			printf("    %d:LStart: %d\n",i,ptrInstrument->sample[i].loopstart);
			printf("    %d:LEnd..: %d\n",i,ptrInstrument->sample[i].looplen);
			printf("    %d:RNote.: %d\n",i,ptrInstrument->sample[i].relativenote);
			printf("\n");
#endif
		}
	}

	//
	// just skip sample data
	//
	// unsigned int fJump = SetFilePointer(hf,0,NULL,FILE_CURRENT);
	unsigned int readBytes;

	for (i=0;i<ptrInstrument->samples;i++)
	{
		readBytes = ptrInstrument->sample[i].len;

		//
		// always allocate 16 bit samples
		//
		ptrInstrument->sample[i].data = new short[readBytes*2];

		//
		// VERIFY THIS!!!  - seems like the sample[i].len is in bytes ???
		//

		if (ptrInstrument->sample[i].looptype & 16)
			readBytes *= 1;

		//
		// load 8 bit and convert to 16 bit, on the fly
		//
		if (!(ptrInstrument->sample[i].looptype & 16))
		{
			char *dummy;

			dummy = new char[readBytes];
			ReadFile(f,dummy,readBytes,&dwRead,NULL);
			for (j=0;j<readBytes;j++)
				ptrInstrument->sample[i].data[j] = dummy[j] << 8;

			delete []dummy;
		}
		else
		{
			ReadFile(f,ptrInstrument->sample[i].data,readBytes,&dwRead,NULL);
		}

		//
		// delta convert sample here
		//
		signed short oldVal,newVal;
		oldVal = 0;
		for (j=0;j<ptrInstrument->sample[i].len;j++)
		{
			newVal = ptrInstrument->sample[i].data[j] + oldVal;
			ptrInstrument->sample[i].data[j] = newVal;
			oldVal = newVal;
		}
	}
	
	// SetFilePointer(hf,fJump,NULL,FILE_BEGIN);

	return true;
}

//////////////////////////////////////////////////////////////////////////
//
// decode pattern data, see top of file comment for decoding sceheme
//
bool XMFile::decodePattern(XM_PATTERN *ptrPattern, unsigned char *ptrData)
{
	unsigned char b;
	int i,j,k;
	struct XM_NOTE *ptrNote;

	for (j=0;j<ptrPattern->rows;j++)
	{

		for(i=0;i<xmHeader.channels;i++)
		{
			ptrNote = &ptrPattern->note_data[i+j*xmHeader.channels];
			memset(ptrNote,0,sizeof(struct XM_NOTE));
			b = *ptrData++;
			if (b & 0x80)
			{
				if (b&1) ptrNote->note   = *ptrData++;
				if (b&2) ptrNote->instr  = *ptrData++;
				if (b&4) ptrNote->volume = *ptrData++;
				if (b&8) ptrNote->effect = *ptrData++;
				if (b&16) ptrNote->effectdata = *ptrData++;

			} else
			{
				if (b) ptrNote->note = b;

				ptrNote->instr  = *ptrData++;
				ptrNote->volume = *ptrData++;
				ptrNote->effect = *ptrData++;
				ptrNote->effectdata = *ptrData++;
			}

			if (ptrNote->note > 0x80)
				ptrNote->note = 0;
		}
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
//
// reste all notes...
//
void XMFile::resetAllNotes()
{
	int i;
	for (i=0;i<255;i++)
		memset(&fxNote[i],0,sizeof(struct XMChannelEffect));
}
//////////////////////////////////////////////////////////////////////////
//
// load a XM File, quite long function
//
bool XMFile::load(char *filename)
{
	FILE *f;
	unsigned int dwRead,i;

//	hf = CreateFile(filename,GENERIC_READ,0,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
	f = fopen(filename, "r");
	if (f==NULL)
		return false;

	if (!ReadFile(f,&xmHeader,sizeof(XM_MODHEAD),&dwRead,NULL))
		goto exitFail;

	if (strncmp(xmHeader.id,"Extended Module: ",17))
	{
#ifdef _DEBUG
		printf("[ERROR]: Unknown ID, Not an XM module!\n");
#endif
		goto exitFail;
	}

	if (xmHeader.version != 0x104) 
	{
		printf("[ERROR]: No support for version: %x\n",xmHeader.version);
		goto exitFail;
	}



#ifdef _DEBUG
	printf("ID.........: %.17s\n",xmHeader.id);
	printf("Tracker....: %.20s\n",xmHeader.tracker);
	printf("Version....: %x\n",xmHeader.version);
	printf("Name.......: %.20s\n",xmHeader.name);
	printf("Channels...: %d\n",xmHeader.channels);
	printf("Song len...: %d\n",xmHeader.songlen);
	printf("Patterns...: %d\n",xmHeader.patterns);
	printf("Instruments: %d\n",xmHeader.instruments);
	printf("Def Tempo..: %d\n",xmHeader.deftempo);
	printf("Def BPM....: %d\n",xmHeader.defbpm);
	printf("FreqTable..: %s\n",(xmHeader.freqtable & 1) ? "Linear" : "Amiga");
	printf("HSize......: %d (struct: %d)\n",xmHeader.headersize + 60,dwRead);
#endif

	if (!(xmHeader.freqtable & 1))
	{
		printf("Amiga frequency tables not supported...\n");
		xmHeader.freqtable |= 1;
	}
	//
	// read patterns
	//

	ptrPatterns = new struct XM_PATTERN[xmHeader.patterns];
	
	dprintf("Reading %d patterns\n",xmHeader.patterns);

	for (i=0;i<xmHeader.patterns;i++)
	{
		dwRead = sizeof(struct XM_PATTERN);
		dwRead -= sizeof(struct XM_NOTE *);
		if (!ReadFile(f,&ptrPatterns[i],sizeof(struct XM_PATTERN)-sizeof(struct XM_NOTE *),&dwRead,NULL))
		{
			printf ("[ERROR]: Failed to read pattern: %d\n",i);
			goto exitFail;
		}
		if (ptrPatterns[i].packingtype != 0)
		{
			
			printf("[WARNING]: Pattern packing type is not 0, should be: %d\n",ptrPatterns[i].packingtype);
			printf("Pattern: %d, size: %d\n",i,ptrPatterns[i].packedpatternsize);
			printf("Header : %d, rows: %d\n",ptrPatterns[i].headersize,ptrPatterns[i].rows);
			goto exitFail;
		}
		if (ptrPatterns[i].packedpatternsize)
		{
			// we have pattern data
			// read and uncrunch pattern data
			unsigned char *tmpPointer;
			tmpPointer = new unsigned char[ptrPatterns[i].packedpatternsize];
			ptrPatterns[i].note_data = new struct XM_NOTE[xmHeader.channels * ptrPatterns[i].rows];
			if (!ReadFile(f, tmpPointer,ptrPatterns[i].packedpatternsize,&dwRead,NULL))
			{
				printf ("[ERROR]: Failed to read pattern data for pattern: %d\n",i);
				delete []tmpPointer;
				goto exitFail;
			}
			if (!decodePattern(&ptrPatterns[i],tmpPointer))
			{
				printf("[ERROR]: Unable to decode pattern: %d\n",i);
				delete []tmpPointer;
				goto exitFail;
			}
			delete []tmpPointer;


		}
	}
	
	
	//
	// now read instrument information
	//
	dprintf("Reading %d instruments\n",xmHeader.instruments);
	ptrInstruments = new struct XM_INSTRUMENT[xmHeader.instruments];
	for (i=0;i<xmHeader.instruments;i++)
	{
		unsigned int tmpOfs;
//		tmpOfs = SetFilePointer(hf,0,NULL,FILE_CURRENT);
		tmpOfs = ftell(f);
//		dprintf("tmpOfs %d\n",tmpOfs);
		if (!ReadFile(f,&ptrInstruments[i],sizeof(struct XM_INSTRUMENT) - sizeof(void *)*2,&dwRead,NULL))
		{
			printf("[error] failed while reading instrument...\n");
			goto exitFail;
		}
		//
		// Set to NULL, we might want to check for validty later...
		//
		ptrInstruments[i].instrextra = NULL;
		ptrInstruments[i].sample = NULL;
		if (ptrInstruments[i].samples)
		{
			dprintf ("Inst: %d\n",i);
			if (!readInstrumentData(f,&ptrInstruments[i],tmpOfs))
			{
				printf("[error]: Failed while reading contents of instrument: %d\n",i);
				goto exitFail;
			}
		}
		else
		{
			dprintf("instrument samples not valid (%d)\n",ptrInstruments[i].sample);
			tmpOfs += ptrInstruments[i].hsize;
//			SetFilePointer(hf,tmpOfs,NULL,FILE_BEGIN);
			fseek(f, tmpOfs, SEEK_SET);
		}

	}
//	CloseHandle(hf);
	fclose(f);
	hasFile = true;
	//
	// reset everything and prepare for play start
	//
	reset();
	return true;

exitFail:
	//CloseHandle(hf);
	fclose(f);
	return false;
}
//////////////////////////////////////////////////////////////////////////
//
// set the stream active
//
void XMFile::setStreamActive(bool activeate /* = true */)
{
	if (activeate)
	{
		assert(ptrMixer);
		ptrMixer->setChannelBuffersize(dwSamplesPerTick);
	}
	Stream::setStreamActive(activeate);
}
//////////////////////////////////////////////////////////////////////////
//
// disable all channels at once
//
void XMFile::muteAll()
{
	int i;
	for (i=0;i<255;i++)
		disableChannel(i);
}
//////////////////////////////////////////////////////////////////////////
//
// enable one specified channel
//
void XMFile::enableChannel(int ch)
{
	fxNote[ch].outChannel.mute(false);
}
//////////////////////////////////////////////////////////////////////////
//
// disable (mute) one specific channel
//
void XMFile::disableChannel(int ch)
{
	fxNote[ch].outChannel.mute(true);
}

//////////////////////////////////////////////////////////////////////////
//
// reset playing
//
void XMFile::reset()
{
	if (!hasFile)
		return;

	glbVolume = 64;
	dwCurrentOrder = 0;
	dwCurrentRow = 0;
	dwPatternDelay = 0;
	dwCurrentPattern = xmHeader.ordertable[dwCurrentOrder];
	isPlaying = false;

	dwCurrentTick = 0;

	dwTempo = xmHeader.deftempo;
	setSpeed(xmHeader.defbpm);

	for (int i=0;i<xmHeader.channels;i++)
		fxNote[i].outChannel.reset(false);

	if (ptrMixer)
		ptrMixer->addStreamChannel(xmHeader.channels);

	resetAllNotes();
	
}
//////////////////////////////////////////////////////////////////////////
//
// calculate and set speed
//
void XMFile::setSpeed(int newSpeed)
{
	float hz;
	if (newSpeed <= 0x1f)
	{
		dwTempo = newSpeed;
		return;
	} 

	hz = (float)dwTempo * 2.0f / 5.0f;
	dwSamplesPerTick = 5 * 44100 / (newSpeed * 2);
	printf("SamplesPerTick: %d\n",dwSamplesPerTick);
	//
	// this might not always work...
	//


	if (ptrMixer)
		ptrMixer->setChannelBuffersize(dwSamplesPerTick);

	// printf ("XM: Samples per tick: %d\n",dwSamplesPerTick);
}

//
// ripped conversion routines
//
#define FMUSIC_XMLINEARPERIOD2HZ(_per) ( (float)(8363.0f*pow(2.0f, ((6.0f*12.0f*16.0f*4.0f - _per) / (float)(12*16*4)))) )
#define FMUSIC_PERIOD2HZ(_per) (14317056L / (_per))

//////////////////////////////////////////////////////////////////////////
//
// return the frequency
//
float XMFile::getFrequency(unsigned int note, int fineTune)
{

	unsigned int period;
	float frequency;

	period = 10*12*16*4 - note*16*4 - fineTune/2;
	frequency = FMUSIC_XMLINEARPERIOD2HZ(period);

	return frequency;
}
//////////////////////////////////////////////////////////////////////////
//
//  returns the playing info
//
bool XMFile::getPlayInfo(unsigned int *pOrder, unsigned int *pPattern, unsigned int *pPatPos)
{
	*pOrder = dwCurrentOrder;
	*pPattern = dwCurrentPattern;
	*pPatPos = dwCurrentRow;

	return true;
}

//////////////////////////////////////////////////////////////////////////
//
// fix handling of glbVolume, done!
//
void XMFile::setChannelVolume(int ch)
{
	float finalVolume;

	finalVolume = (float)fxNote[ch].vol;
	finalVolume *= fxNote[ch].envvol;
	finalVolume *= fxNote[ch].fadeoutvol;
	finalVolume *= glbVolume;

	finalVolume *= (64.0f / (64.0f * 64.0f * 65536.0f * 64.0f));

	fxNote[ch].outChannel.setVolume(((float)finalVolume) / 32.0f); // * glbVolume);
}

//////////////////////////////////////////////////////////////////////////
//
// process vibrato, mimifmod...  
//
void XMFile::processVibrato(int ch)
{
	int delta;

	switch(fxNote[ch].vibwave)
	{
	case 0 :
		delta = (int)(fabs ((sin( (float)(fxNote[ch].vibpos) * 2 * 3.141592 / 64.0f )) * 256.0f));
		break;
	}

	delta *= fxNote[ch].vibdepth;
	delta >>=7;
	delta <<=2;   // we use 4*periods so make vibrato 4 times bigger



	if (fxNote[ch].vibpos >= 0)
		fxNote[ch].freqdelta = -delta;
	else
		fxNote[ch].freqdelta = delta;

	fxNote[ch].chControl |= XM_CH_FREQ;

}

//////////////////////////////////////////////////////////////////////////
//
// slide volume up
//
void XMFile::slideVolumeUp(int ch, int count)
{
	fxNote[ch].vol += count;
	if (fxNote[ch].vol > 0x40)
		fxNote[ch].vol = 0x40;

	fxNote[ch].chControl |= XM_CH_SET_VOL;

}

//////////////////////////////////////////////////////////////////////////
//
// slide volume down, count must be postive!
//
void XMFile::slideVolumeDown(int ch, int count)
{
	fxNote[ch].vol -= count;
	if (fxNote[ch].vol < 0)
		fxNote[ch].vol = 0;

	fxNote[ch].chControl |= XM_CH_SET_VOL;
}

//////////////////////////////////////////////////////////////////////////
//
// arbitary slide volume
//
void XMFile::slideVolume(int ch)
{
	fxNote[ch].vol += fxNote[ch].voldelta;

	if (fxNote[ch].vol > 0x40)
		fxNote[ch].vol = 0x40;

	if (fxNote[ch].vol < 0)
		fxNote[ch].vol = 0;

	fxNote[ch].chControl |= XM_CH_SET_VOL;
}

//////////////////////////////////////////////////////////////////////////
//
// process portamento
//
void XMFile::processPortamento(int ch)
{

	if (fxNote[ch].freq < fxNote[ch].portatarget)
	{
		fxNote[ch].freq += fxNote[ch].portaspeed * 4;
		if (fxNote[ch].freq > fxNote[ch].portatarget)
			fxNote[ch].freq= fxNote[ch].portatarget;
	} else
	{
		fxNote[ch].freq -= fxNote[ch].portaspeed * 4;
		if (fxNote[ch].freq < fxNote[ch].portatarget)
			fxNote[ch].freq = fxNote[ch].portatarget;
	}

	fxNote[ch].chControl |= XM_CH_FREQ;
}

void XMFile::processSliding(int ch)
{
	if (fxNote[ch].portatarget > 0)
	{
		fxNote[ch].freq += fxNote[ch].portaspeed * 4;
	} else
	{
		fxNote[ch].freq -= fxNote[ch].portaspeed * 4;
	}

	fxNote[ch].chControl |= XM_CH_FREQ;
}

//////////////////////////////////////////////////////////////////////////
//
// process volume fx
//
void XMFile::processVolumeFx(int ch, unsigned char vol)
{
	
	if ((vol >= 0x10) && (vol <= 0x50))
	{
		fxNote[ch].vol = vol - 0x10;
		fxNote[ch].chControl |= XM_CH_SET_VOL;
	} else
	{
		int pVol,nVol;
		pVol = fxNote[ch].vol; //(unsigned int)(64.0f * fxNote[ch].outChannel.getVolume());

		switch (vol >> 4)
		{
		case 0x06:
			nVol = pVol - (vol & 0x0f);
			if (nVol < 0) nVol = 0;
			fxNote[ch].chControl |= XM_CH_SET_VOL;
			break;
		case 0x07:
			nVol = pVol + (vol & 0x0f);
			if (nVol > 64) nVol = 64;
			fxNote[ch].vol = nVol;
			fxNote[ch].chControl |= XM_CH_SET_VOL;
			break;
		case 0x08:
			nVol = pVol - (vol & 0x0f);
			if (nVol < 0) nVol = 0;
			fxNote[ch].vol = nVol;
			fxNote[ch].chControl |= XM_CH_SET_VOL;
			break;
		case 0x09:
			nVol = pVol + (vol & 0x0f);
			if (nVol >= 64) nVol = 64;
			fxNote[ch].vol = nVol;
			fxNote[ch].chControl |= XM_CH_SET_VOL;
			break;
		case 0x0a :	// vibrato speed
			fxNote[ch].vibspeed = vol & 0x0f;
			fxNote[ch].chControl |= XM_CH_VIBRATO;
			processVibrato(ch);				// ? process on tick 0
			break;
		case 0x0b :	// vibrato
			fxNote[ch].vibdepth = vol & 0x0f;
			fxNote[ch].chControl |= XM_CH_VIBRATO;
			processVibrato(ch);				// ? process on tick 0
			break;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
//
// this is identical to the "minifmod" code, actually it is ripped...
// and, eh, it does not support "keyoff"...
//
void XMFile::processEnvelope(int ch)
{
	struct XM_INSTRUMENT_EXTRA *ptrExtra;
	ptrExtra = fxNote[ch].ptrInstrExtra;
	int pos;

	if (!ptrExtra)
		return;

	if (fxNote[ch].envvolpos < ptrExtra->numvolenvpoints)
	{
		if (fxNote[ch].envvoltick == ptrExtra->volenvpoints[fxNote[ch].envvolpos*2])
		{
			int currtick, nexttick;
			int currpos, nextpos;
			int currval, nextval;
			int tickdiff;

			if ((ptrExtra->voltype & XM_ENVELOPE_LOOP) && (fxNote[ch].envvolpos == ptrExtra->volloopend)) 
			{
				fxNote[ch].envvolpos = ptrExtra->volloopstart;
				fxNote[ch].envvoltick = ptrExtra->volenvpoints[fxNote[ch].envvolpos * 2];
			}


			currpos = fxNote[ch].envvolpos;
			nextpos = fxNote[ch].envvolpos+1;
			currtick = ptrExtra->volenvpoints[currpos*2];
			nexttick = ptrExtra->volenvpoints[nextpos*2];

			currval = ptrExtra->volenvpoints[currpos*2+1];
			nextval = ptrExtra->volenvpoints[nextpos*2+1];


			if (currpos == ptrExtra->numvolenvpoints-1)
			{
				fxNote[ch].envvol = currval;
				fxNote[ch].chControl |= XM_CH_SET_VOL;
				fxNote[ch].envvolstopped = true;
				return;
			}

			if ((ptrExtra->voltype & XM_ENVELOPE_SUSTAIN) && (fxNote[ch].envvolpos == ptrExtra->volsustainpoint) && (!fxNote[ch].keyoff))
			{
				fxNote[ch].envvol = currval;
				fxNote[ch].chControl |= XM_CH_SET_VOL;
				return;
			}

	
			tickdiff = nexttick - currtick;
			if (tickdiff)
				fxNote[ch].envvoldelta = ((float)(nextval - currval)) / (float)tickdiff;
			else
				fxNote[ch].envvoldelta = 0.0f;

			fxNote[ch].envvolipol = (float)currval;
			fxNote[ch].envvolpos++;

		} else
		{
			fxNote[ch].envvolipol += fxNote[ch].envvoldelta;
		}
	}

	fxNote[ch].envvol = (int)fxNote[ch].envvolipol;
	fxNote[ch].envvoltick++;
	fxNote[ch].chControl |= XM_CH_SET_VOL;
}

//////////////////////////////////////////////////////////////////////////
//
// set current order number
//
void XMFile::setCurrentOrder(unsigned int newOrder)
{
	dwCurrentOrder = newOrder;
}
//////////////////////////////////////////////////////////////////////////
//
// reset the channel data, channel data are temporary variables that
// are used in-between note updates to hold states...
//
void XMFile::resetXMChannel(int ch)
{
	fxNote[ch].vibpos = 0;
	fxNote[ch].vibwave = 0;

	fxNote[ch].chControl  = 0;
	fxNote[ch].freqdelta = 0;
//	fxNote[ch].freq = 0;
}

//////////////////////////////////////////////////////////////////////////
//
// the big doTick function, called when cursor has advanced to a new 
// position, we suck in all values into holding structures (to enable
// us smooth operation during the in-between tick updates)
//
// also does all printing...
//
void XMFile::doTick()
{
	int i,oldfreq;
	unsigned int dwNewRow;
	bool breakPattern = false;
	struct XM_PATTERN *ptrPattern;
	struct XM_NOTE *ptrRow;
	unsigned int dwNote;

	dwCurrentPattern = xmHeader.ordertable[dwCurrentOrder];
	ptrPattern = &ptrPatterns[dwCurrentPattern];
	ptrRow = &ptrPattern->note_data[dwCurrentRow * xmHeader.channels];


	dprintf ("%.2x|",dwCurrentRow);
	for (i=0;i<xmHeader.channels;i++)
	{
		struct XM_SAMPLE *ptrSample = NULL;

		if (fxNote[i].outChannel.getMute())
			continue;

		resetXMChannel(i);
		//
		// copy note to fx place
		//
		fxNote[i].note = ptrRow[i];
		//
		// process some global volume effects, this need to be done first...
		//
		switch(ptrRow[i].effect)
		{
		case 0x10:
			glbVolume = (int)(unsigned int)(ptrRow[i].effectdata);
			break;
		}

		//
		//////////////////////////////////////////////////////////////////////////
		//
		dwNote = ptrRow[i].note;

		if (ptrRow[i].instr)
		{
			unsigned int dwSample;
			unsigned char instr;
			
			instr = ptrRow[i].instr-1;

			if (ptrInstruments[instr].instrextra)
			{
				dwSample = ptrInstruments[instr].instrextra->votesmpnumber[dwNote-1];
				ptrSample = &ptrInstruments[instr].sample[dwSample];

				fxNote[i].outChannel.reset();
				fxNote[i].outChannel.setDataStream(ptrSample->data,ptrSample->len,Channel::f16BitSigned);
				fxNote[i].outChannel.setLoopParameters(false,0,0);

				//
				// TODO: need to add proper support for different loop styles...
				//
				if ((ptrSample->looptype & 1) && (ptrSample->looplen > 0))
					fxNote[i].outChannel.setLoopParameters(true,ptrSample->loopstart,ptrSample->looplen);

				if ((ptrSample->looptype & 2) && (ptrSample->looplen > 0))
					fxNote[i].outChannel.setLoopParameters(true,ptrSample->loopstart,ptrSample->looplen);

				fxNote[i].vol = (int)(unsigned int)ptrSample->vol;
				setChannelVolume(i);

				fxNote[i].ptrSample = ptrSample;
				fxNote[i].ptrInstrExtra = ptrInstruments[instr].instrextra;
				fxNote[i].envvol = 64;
				fxNote[i].envvolpos = 0;
				fxNote[i].envvoltick = 0;
				fxNote[i].envvolstopped = false;
				fxNote[i].keyoff = false;
				fxNote[i].fadeoutvol = 65536;
				fxNote[i].freqdelta = 0;

				fxNote[i].chControl |= XM_CH_SET_VOL;
			} 
					
		} else
			ptrSample = fxNote[i].ptrSample; // fetch old note for sample

		if (ptrRow[i].effect == 0x04)
		{
			// break here
			int i;
			i = 0;
		}

		oldfreq = fxNote[i].period;
		if (dwNote && (dwNote != XM_KEYOFF))
		{
			dprintf("%.3s ",glbNoteStrings[dwNote]);
			if (ptrSample)
			{
				oldfreq = fxNote[i].period;
				fxNote[i].period = 10*12*16*4 - ((dwNote-1) + ptrSample->relativenote)*16*4 - ptrSample->finetune/2;
				fxNote[i].freq = fxNote[i].period;
				fxNote[i].freqdelta = 0;
				fxNote[i].chControl |= XM_CH_FREQ;
			}
			fxNote[i].keyoff = false;
		}
		else
		{
			// handle key off
			if (dwNote == XM_KEYOFF)
			{
				dprintf("OFF ");
				//
				// skip this if we dont have any extra information about the instrument
				//
				if (fxNote[i].ptrInstrExtra)
					fxNote[i].keyoff = true;
			} else
			{
				dprintf ("--- ");
			}			
			dwNote = 0;	// dont handle 
		}


		//
		// ProcessVolumeEnvelope
		//

		if (fxNote[i].ptrInstrExtra)
		{
			if ((fxNote[i].ptrInstrExtra->voltype & XM_ENVELOPE_ACTIVE) && (!fxNote[i].envvolstopped))
				processEnvelope(i);
		}

		//
		// process keyoff at first tick?
		//
		if (fxNote[i].keyoff)
		{

			fxNote[i].fadeoutvol -= fxNote[i].ptrInstrExtra->volfadeout;
			if (fxNote[i].fadeoutvol < 0)
				fxNote[i].fadeoutvol = 0;

			fxNote[i].chControl |= XM_CH_SET_VOL;
		}


		//
		// process volume effect
		//
		if (ptrRow[i].volume)
		{
			unsigned char vol;
			vol = ptrRow[i].volume;

			dprintf ("%.2x ",ptrRow[i].volume);

			fxNote[i].chControl |= XM_CH_VOLUME_FX;
			processVolumeFx(i,vol);
		} else
		{
			dprintf ("-- ");
		}


		//
		// parse commands
		//
		unsigned int fxParamX, fxParamY;
		fxParamX = (ptrRow[i].effectdata >> 4) & 0x0f;
		fxParamY = ptrRow[i].effectdata & 0x0f;
		switch(ptrRow[i].effect)
		{
		case 0x01: // Slide up

			fxNote[i].portatarget = -1;
			if (ptrRow[i].effectdata)
				fxNote[i].portaspeed = ptrRow[i].effectdata;
			
			fxNote[i].chControl |= XM_CH_SLIDE;
			fxNote[i].chControl &= ~XM_CH_FREQ;

			dprintf("SU%.1x%.1x",fxParamX,fxParamY);
			break;
		case 0x02: // Slide down

			fxNote[i].portatarget = 1;

			if (ptrRow[i].effectdata)
				fxNote[i].portaspeed = ptrRow[i].effectdata;

			fxNote[i].chControl |= XM_CH_SLIDE;
			fxNote[i].chControl &= ~XM_CH_FREQ;

			dprintf("SD%.1x%.1x",fxParamX,fxParamY);
			break;
		case 0x03: // Tone portamento
			fxNote[i].portatarget = fxNote[i].period;

			//fxNote[i].freq = oldfreq;
			//fxNote[i].period = oldfreq;

			if (ptrRow[i].effectdata)
				fxNote[i].portaspeed = ptrRow[i].effectdata;

			fxNote[i].chControl |= XM_CH_PORTAMENTO;

			//
			// dont fiddle with frequency
			//
			fxNote[i].chControl &= ~XM_CH_FREQ;

			dprintf("P %.1x%.1x",fxParamX,fxParamY);
			break;
		case 0x04: // vibrato
			if (fxParamX)
				fxNote[i].vibspeed = fxParamX;
			if (fxParamY)
				fxNote[i].vibdepth = fxParamY;

			fxNote[i].chControl |= XM_CH_VIBRATO;
			processVibrato(i);
			dprintf("V %.1x%.1x",fxParamX,fxParamY);
			break;
		case 0x06: // continue vibrato, but do volume slide aswell
			fxNote[i].chControl |= XM_CH_VIBRATO;
			fxNote[i].chControl |= XM_CH_VOL_SLIDE;
			//
			// both cannot be non-zero, we dont care...
			//
			if (fxParamX)
				fxNote[i].voldelta = (int)fxParamX;

			if (fxParamY)
				fxNote[i].voldelta = -(int)fxParamY;

			processVibrato(i);
			dprintf("VS%.1x%.1x",fxParamX,fxParamY);

			break;
		case 0x09:	// set sample offset
			{
				unsigned int offset;
				offset = fxParamX * 4096 + fxParamY * 256;
				fxNote[i].outChannel.setStreamOffset(offset);			
			}
			dprintf("O %.1x%.1x",fxParamX,fxParamY);
			break;
		case 0x0a:	// slide volume
			fxNote[i].chControl |= XM_CH_VOL_SLIDE;
			if(!ptrRow[i].effectdata)
			{
				fxNote[i].voldelta = fxNote[i].volSlideCmdLastUsed;
			} else
			{
				if(fxParamX) fxNote[i].voldelta = (int)fxParamX;
				if(fxParamY) fxNote[i].voldelta = -(int)fxParamY;

				fxNote[i].volSlideCmdLastUsed = fxNote[i].voldelta;
			}

			dprintf("A %.1x%.1x",fxParamX,fxParamY);
			break;
		case 0x0c:	// volume command
			fxNote[i].vol = ptrRow[i].effectdata;
			if (fxNote[i].vol > 64)
				fxNote[i].vol = 64;
			fxNote[i].chControl |= XM_CH_SET_VOL;
			dprintf("C %.1x%.1x",fxParamX,fxParamY);
			break;
		case 0x0d:
			breakPattern = true;
			dwNewRow = fxParamX * 10 + fxParamY;
			dprintf("PB%.1x%.1x",fxParamX,fxParamY);
			break;
		case 0x0e:
			switch(fxParamX)
			{
			case 0x0d :
				fxNote[i].chControl |= XM_CH_NOTE_DELAY;
				fxNote[i].tickDelayForNote = fxParamY;
				if (fxParamY)
					fxNote[i].outChannel.setActive(false);

				dprintf("ND%.1x%.1x",fxParamX,fxParamY);
				break;
			default:
				dprintf("E %.1x%.1x",fxParamX,fxParamY);
				break;
			}
			break;
		case 0x0f:	 // speed command
			dprintf("S %.2x",ptrRow[i].effectdata);
			setSpeed(ptrRow[i].effectdata);
			break;
		case 0x10:
			dprintf("GS%.2x",ptrRow[i].effectdata);
			//glbVolume = ptrRow[i].effectdata / 64.0f;
			break;
		default:
			dprintf("%.2x%.2x",ptrRow[i].effect,ptrRow[i].effectdata);
		}
		dprintf ("|");


		if (fxNote[i].chControl & XM_CH_FREQ)
		{
			float _freq;
			if (xmHeader.freqtable & XM_FREQ_LINEAR)
				_freq = FMUSIC_XMLINEARPERIOD2HZ(fxNote[i].freq + fxNote[i].freqdelta);
			else
				_freq = FMUSIC_PERIOD2HZ(fxNote[i].freq + fxNote[i].freqdelta);


			if (_freq < 100)
				_freq = 100;

			fxNote[i].outChannel.setFreq(_freq);
		}

		if (fxNote[i].chControl & XM_CH_SET_VOL)
			setChannelVolume(i);

	}


	dwCurrentRow++;
	if ((dwCurrentRow >= ptrPattern->rows) || (breakPattern))
	{
		if (breakPattern)
			dwCurrentRow=dwNewRow;
		else
			dwCurrentRow=0;
		dwCurrentOrder++;

		if (dwCurrentOrder >= xmHeader.songlen)
			dwCurrentOrder = xmHeader.restartpos;

		dwCurrentPattern = xmHeader.ordertable[dwCurrentOrder];
		dprintf ("\n");
	}
	dprintf ("\n");


}
//////////////////////////////////////////////////////////////////////////
//
// dump, not used, actual printing in debug mode is done in doTick 
//
void XMFile::printPatternRow(XM_PATTERN *ptrPattern, unsigned int row)
{
	int i;
	struct XM_NOTE *ptrNote;
	for (i=0;i<ptrPattern->rows;i++)
	{
		ptrNote = &ptrPattern->note_data[row+i*xmHeader.channels];
		printf ("%.2d | %.2d - %.2x %.2x %.2x:%.2x\n",i, ptrNote->note,
						  ptrNote->instr,
						  ptrNote->volume,
						  ptrNote->effect,
						  ptrNote->effectdata);
	}
}

//////////////////////////////////////////////////////////////////////////
//
// update the effects, done in-between ticks...
//
void XMFile::updateEffects()
{
	int i;
	for (i=0;i<xmHeader.channels;i++)
	{
		if (fxNote[i].chControl & XM_CH_NOTE_DELAY)
		{
			if (fxNote[i].tickDelayForNote)
			{
				// process other stuff anyway ??
				fxNote[i].tickDelayForNote--;
				continue;
			}
			fxNote[i].outChannel.setActive(true);
		}

		if (fxNote[i].chControl & XM_CH_VOLUME_FX)
			processVolumeFx(i,fxNote[i].note.volume);


		if (fxNote[i].chControl & XM_CH_VOL_SLIDE)
			slideVolume(i);

		if (fxNote[i].chControl & XM_CH_PORTAMENTO)
		{
			processPortamento(i);
		}

		if (fxNote[i].chControl & XM_CH_SLIDE)
		{
			processSliding(i);
		}


		if (fxNote[i].chControl & XM_CH_VIBRATO)
		{
			processVibrato(i);

			fxNote[i].vibpos += fxNote[i].vibspeed;
			if (fxNote[i].vibpos > 31) 
				fxNote[i].vibpos -= 64;
		}

		if(fxNote[i].ptrInstrExtra)
		{
			if ((fxNote[i].ptrInstrExtra->voltype & XM_ENVELOPE_ACTIVE) && (!fxNote[i].envvolstopped))
				processEnvelope(i);
		}

		if (fxNote[i].keyoff)
		{
			// TODO: need to take care of envelopes
			fxNote[i].fadeoutvol -= fxNote[i].ptrInstrExtra->volfadeout;
			if (fxNote[i].fadeoutvol < 0)
				fxNote[i].fadeoutvol = 0;

			fxNote[i].chControl |= XM_CH_SET_VOL;
		}

		if (fxNote[i].chControl & XM_CH_SET_VOL)
			setChannelVolume(i);


		if (fxNote[i].chControl & XM_CH_FREQ)
		{
			float _freq;

			if (xmHeader.freqtable & XM_FREQ_LINEAR)
				_freq = FMUSIC_XMLINEARPERIOD2HZ(fxNote[i].freq + fxNote[i].freqdelta);
			else
				_freq = FMUSIC_PERIOD2HZ(fxNote[i].freq + fxNote[i].freqdelta);

			if (_freq < 100)
				_freq = 100;

			fxNote[i].outChannel.setFreq(_freq);
		}

	}
}

//////////////////////////////////////////////////////////////////////////
//
// render samples...
// currently the mixer output buffers and the file render buffer need
// to be aligned...
//
bool XMFile::render(unsigned int dwNumSamples)
{
	int i;


	for (i=0;i<xmHeader.channels;i++)
	{
		fxNote[i].outChannel.resetWritePosition();
	}
		
	//
	// redo this to be independent from buffer size synchronization
	// we should be able to render as much data as needed...
	//	
	int count = dwNumSamples;
	int num,tot;
	tot = 0;

	if (iSamplesLeft > 0)
	{
		// render the channels to the mixer
		num = iSamplesLeft;
		if (dwNumSamples > num)
		{
			num = dwNumSamples;
		} 

		for (i=0;i<xmHeader.channels;i++)
		{
			fxNote[i].outChannel.render(ptrMixer,iSamplesLeft);
		}
		count -= iSamplesLeft;
		tot += iSamplesLeft;
	}	

	num = dwSamplesPerTick;
	while (count > 0)
	{
		if (!dwCurrentTick)
		{
			doTick();
		}
		else
		{
			updateEffects();
		}

		if (count > dwSamplesPerTick)
		{
			num = dwSamplesPerTick;
		} else
		{
			num = count;
		}

		// render the channels to the mixer
		for (i=0;i<xmHeader.channels;i++)
		{
			fxNote[i].outChannel.render(ptrMixer,num);
		}

		// update tick info
		dwCurrentTick++;
		if (dwCurrentTick >= (dwTempo + dwPatternDelay))
		{
			dwCurrentTick = 0;
			dwPatternDelay = 0;
		}
		tot += dwSamplesPerTick;
		count -= dwSamplesPerTick;
	}
	iSamplesLeft = tot - dwNumSamples;

	//dprintf("L: %d\n",iSamplesLeft);

	return false;
}

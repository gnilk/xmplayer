#include <stdio.h>
#include "filexm.h"



#include <CoreServices/CoreServices.h>
#include <stdio.h>
#include <unistd.h>
#include <AudioUnit/AudioUnit.h>
#include <CoreServices/CoreServices.h>
#include <CoreAudio/CoreAudio.h>

#include <math.h>

enum {
	kAsFloat = 32,
	kAs16Bit = 16,
	kAs24Bit = 24
};

Float64			sSampleRate = 44100;
int				sNumChannels = 2;
int				sWhichFormat = kAsFloat;
UInt32 theFormatID = kAudioFormatLinearPCM;

// these are set based on which format is chosen
UInt32 theFormatFlags = 0;
UInt32 theBytesInAPacket = 0;
UInt32 theBitsPerChannel = 0;
UInt32 theBytesPerFrame = 0;

// these are the same regardless of format
UInt32 theFramesPerPacket = 1; // this shouldn't change


AudioUnit	gOutputUnit;
static Mixer *glb_pMixer;
static XMFile *glb_pXMFile;

OSStatus	MyRenderer(void 				*inRefCon, 
					   AudioUnitRenderActionFlags 	*ioActionFlags, 
					   const AudioTimeStamp 		*inTimeStamp, 
					   UInt32 						inBusNumber, 
					   UInt32 						inNumberFrames, 
					   AudioBufferList 			*ioData)

{
/*	
	RenderSin (sSinWaveFrameCount, 
			   inNumberFrames,  
			   ioData->mBuffers[0].mData, 
			   sSampleRate, 
			   sAmplitude, 
			   sToneFrequency, 
			   sWhichFormat);
*/	
	
	//we're just going to copy the data into each channel
//	for (UInt32 channel = 1; channel < ioData->mNumberBuffers; channel++)
//		memcpy (ioData->mBuffers[channel].mData, ioData->mBuffers[0].mData, ioData->mBuffers[0].mDataByteSize);

	// Perhaps the synchronization should not be done here... =)
	//	while (glb_pMixer->pOutputLeftChannel->GetBytes() < (4 * (int)ioData->mBuffers[0].mDataByteSize))
	while (glb_pMixer->pMonoOutput->GetBytes() < (4 * (int)ioData->mBuffers[0].mDataByteSize))
	{
		// causes the XM player to write data into the output buffer
		glb_pMixer->renderStreams();			
	}
	
	// Dump information about the ring buffer situation
	{
		int rc, wc, bytes, capacity;
		glb_pMixer->pMonoOutput->GetInfo(&rc, &wc, &bytes, &capacity);
//		printf("rc: %.5d, wc: %.5d, bytes: %.5d, capacity: %.5d\n",rc,wc,bytes,capacity);
	}
	
	
	// Fetch data from the output buffer
	for (UInt32 channel = 0; channel < ioData->mNumberBuffers; channel++)
	{
	//	glb_pMixer->pOutputLeftChannel->Peek(ioData->mBuffers[channel].mData, ioData->mBuffers[channel].mDataByteSize);
		glb_pMixer->pMonoOutput->Peek(ioData->mBuffers[channel].mData, ioData->mBuffers[channel].mDataByteSize);
	}

	// Pop the data from the mixer
	// glb_pMixer->pOutputLeftChannel->Pop((int)ioData->mBuffers[0].mDataByteSize);
	glb_pMixer->pMonoOutput->Forward((int)ioData->mBuffers[0].mDataByteSize);
				

	return noErr;
}

// ________________________________________________________________________________
//
// CreateDefaultAU
//
bool CreateDefaultAU()
{

	//AudioComponentFindNext()
	OSStatus err = noErr;
	
	// Open the default output unit
	AudioComponentDescription desc;
	desc.componentType = kAudioUnitType_Output;
	desc.componentSubType = kAudioUnitSubType_DefaultOutput;
	desc.componentManufacturer = kAudioUnitManufacturer_Apple;
	desc.componentFlags = 0;
	desc.componentFlagsMask = 0;
	
	AudioComponent comp = AudioComponentFindNext(NULL, &desc);
	if (comp == NULL) {
		printf ("AudioComponentFindNext returned null\n");
		return false;
	}
	
	err = AudioComponentInstanceNew(comp, &gOutputUnit);
	if (err != noErr) {
		printf ("AudioComponentInstanceNew=%ld\n", (long int)err);
		return false;
	}

	err = AudioUnitInitialize(gOutputUnit);
	if (err != noErr) {
		printf ("AudioUnitInitialize=%ld\n", (long int)err);
		return false;
	}
	
	// Set up a callback function to generate output to the output unit
    AURenderCallbackStruct input;
	input.inputProc = MyRenderer;
	input.inputProcRefCon = NULL;
	
	err = AudioUnitSetProperty (gOutputUnit, 
								kAudioUnitProperty_SetRenderCallback, 
								kAudioUnitScope_Input,
								0, 
								&input, 
								sizeof(input));
	
//	err = AudioUnitAddRenderNotify(gOutputUnit, MyRenderer, NULL);
	if (err != noErr) {
		printf ("AudioUnitSetProperty-CB=%ld\n", (long int)err);
		return false;
	}
	return true;
    
}

// ________________________________________________________________________________
//
// TestDefaultAU
//
void	StartDefaultAU()
{
	OSStatus err = noErr;
	switch (sWhichFormat) 
	{
		case kAsFloat:
			theFormatFlags =  kAudioFormatFlagsNativeFloatPacked | kAudioFormatFlagIsNonInterleaved;
			theBytesPerFrame = theBytesInAPacket = 4;
			theBitsPerChannel = 32;
			break;
			
		case kAs16Bit:
			theFormatFlags =  kLinearPCMFormatFlagIsSignedInteger
			| kAudioFormatFlagsNativeEndian
			| kLinearPCMFormatFlagIsPacked
			| kAudioFormatFlagIsNonInterleaved;
			theBytesPerFrame = theBytesInAPacket = 2;
			theBitsPerChannel = 16;		
			break;
			
		case kAs24Bit:
			theFormatFlags =  kLinearPCMFormatFlagIsSignedInteger 
			| kAudioFormatFlagsNativeEndian
			| kAudioFormatFlagIsNonInterleaved;
			theBytesPerFrame = theBytesInAPacket = 4;
			theBitsPerChannel = 24;
			break;
			
		default:
			printf ("unknown format\n");
			return;
	}
	
    
	// We tell the Output Unit what format we're going to supply data to it
	// this is necessary if you're providing data through an input callback
	// AND you want the DefaultOutputUnit to do any format conversions
	// necessary from your format to the device's format.
	AudioStreamBasicDescription streamFormat;
	streamFormat.mSampleRate = sSampleRate;		//	the sample rate of the audio stream
	streamFormat.mFormatID = theFormatID;			//	the specific encoding type of audio stream
	streamFormat.mFormatFlags = theFormatFlags;		//	flags specific to each format
	streamFormat.mBytesPerPacket = theBytesInAPacket;	
	streamFormat.mFramesPerPacket = theFramesPerPacket;	
	streamFormat.mBytesPerFrame = theBytesPerFrame;		
	streamFormat.mChannelsPerFrame = sNumChannels;	
	streamFormat.mBitsPerChannel = theBitsPerChannel;	
	
	printf("Rendering source:\n\t");
	printf ("SampleRate=%f,", streamFormat.mSampleRate);
	printf ("BytesPerPacket=%ld,", (long int)streamFormat.mBytesPerPacket);
	printf ("FramesPerPacket=%ld,", (long int)streamFormat.mFramesPerPacket);
	printf ("BytesPerFrame=%ld,", (long int)streamFormat.mBytesPerFrame);
	printf ("BitsPerChannel=%ld,", (long int)streamFormat.mBitsPerChannel);
	printf ("ChannelsPerFrame=%ld\n", (long int)streamFormat.mChannelsPerFrame);
	
	err = AudioUnitSetProperty (gOutputUnit,
								kAudioUnitProperty_StreamFormat,
								kAudioUnitScope_Input,
								0,
								&streamFormat,
								sizeof(AudioStreamBasicDescription));
	if (err) { printf ("AudioUnitSetProperty-SF=%4.4s, %ld\n", (char*)&err, (long int)err); return; }
	
    // Initialize unit
	err = AudioUnitInitialize(gOutputUnit);
	if (err) { printf ("AudioUnitInitialize=%ld\n", (long int)err); return; }
    
	Float64 outSampleRate;
	UInt32 size = sizeof(Float64);
	err = AudioUnitGetProperty (gOutputUnit,
								kAudioUnitProperty_SampleRate,
								kAudioUnitScope_Output,
								0,
								&outSampleRate,
								&size);
	if (err) { printf ("AudioUnitSetProperty-GF=%4.4s, %ld\n", (char*)&err, (long int)err); return; }
	
	// Start the rendering
	// The DefaultOutputUnit will do any format conversions to the format of the default device
	err = AudioOutputUnitStart (gOutputUnit);
	if (err) { printf ("AudioOutputUnitStart=%ld\n", (long int)err); return; }
	
	// we call the CFRunLoopRunInMode to service any notifications that the audio
	// system has to deal with
	//CFRunLoopRunInMode(kCFRunLoopDefaultMode, 2, false);
	
	
}

void CloseDefaultAU ()
{
	OSStatus err = noErr;

	// REALLY after you're finished playing STOP THE AUDIO OUTPUT UNIT!!!!!!	
	// but we never get here because we're running until the process is nuked...	
	AudioOutputUnitStop (gOutputUnit);
	
    err = AudioUnitUninitialize (gOutputUnit);
	if (err) { printf ("AudioUnitUninitialize=%ld\n", (long int)err); return; }
	// Clean up
	AudioUnitUninitialize(gOutputUnit);
}

void CreateArgListFromString (int *ioArgCount, char **ioArgs, char *inString)
{
    int i, length;
    length = strlen (inString);
    
    // prime for first argument
    ioArgs[0] = inString;
    *ioArgCount = 1;
    
    // get subsequent arguments
    for (i = 0; i < length; i++) {
        if (inString[i] == ' ') {
            inString[i] = 0;		// terminate string
            ++(*ioArgCount);		// increment count
            ioArgs[*ioArgCount - 1] = inString + i + 1;	// set next arg pointer
        }
    }
}

extern int glb_Verbose;


int main (int argc, const char * argv[]) 
{
		
    // insert code here...
	Mixer *pMixer = new Mixer();
	XMFile *pXMFile = new XMFile();
	pMixer->initialize();
	pMixer->addStream(pXMFile, false);

	glb_Verbose = 0;
	char *sFileName = "vdlove.xm";
	if (argc > 0)
	{
		int i;
		for (i=1;i<argc;i++)
		{
			if (!strcmp(argv[i],"-v"))
			{
				glb_Verbose = 1;
				printf("Is Verbose!\n");
			} else
			{
				sFileName = (char *)argv[i];
			}
		}
	}
	if (!CreateDefaultAU()) {
		fprintf(stderr, "FATAL: Unable to create audio unit\n");
		return 1;
	}

	printf("XM Player for Mac OsX - really lousy playing\n");
	printf("Loading: %s\n",sFileName);
	if (pXMFile->load(sFileName))
	{
		printf("Load ok\n");
		pXMFile->setStreamActive(true);
		
		// PreRender some data
//		for(int i=0;i<100;i++)
//		{
//			pMixer->renderStreams();			
//		}
		glb_pMixer = pMixer;
		glb_pXMFile = pXMFile;
		
		StartDefaultAU();

		printf("Playing...");
		fgets("    ",4,stdin);
		CloseDefaultAU();
		
	} else
	{
		printf("Failed to load 'vdlove.xm'\n");
	}
	
    return 0;
}

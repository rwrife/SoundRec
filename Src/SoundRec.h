/*
 * SoundRec.h
 *
 * header file for SoundRec
 *
 * This wizard-generated code is based on code adapted from the
 * stationery files distributed as part of the Palm OS SDK 4.0.
 *
 * Copyright (c) 1999-2000 Palm, Inc. or its subsidiaries.
 * All rights reserved.
 */
 
#ifndef SOUNDREC_H_
#define SOUNDREC_H_

/*********************************************************************
 * Internal Structures
 *********************************************************************/
typedef struct
{
	char	riff[4]; //riff
	UInt32	fileLen;
	char	wave[4]; //wave
	char	fmt[4];
	UInt32	fmtLen;
	UInt16	fmtTag;  //1-pcm
	UInt16	channels;
	UInt32	sampleRate;
	UInt32	bytesSec;
	UInt16	blockAlign;
	UInt16	bits;
	UInt16	extraBytes;
	char 	fact[4];
	UInt32	factSize;
	UInt32	factData;
	char	data[4];	//data
	UInt32	dataLen;
} WavType;

/*********************************************************************
 * Global variables
 *********************************************************************/


/*********************************************************************
 * Internal Constants
 *********************************************************************/

#define appFileCreator			'RRSR'
#define appName					"SoundRec"
#define appVersionNum			0x01
#define appPrefID				0x00
#define appPrefVersionNum		0x02
#define libPrefID				0
#define sndDBName				"SoundRec-Sound"
#define recDBName				"SoundRec-Recording"
#define SDAudioDir				"/PALM/Programs/SoundRec"
#define	libDBType				'DATA'
#define	libCreatorID			'RRSR'

static UInt8 	SoundVersion 	= 106;
static UInt16	cardId 			= 0;

typedef enum 
{
	none,
	record,
	play,
	busy,
	pause
}sndAction;

struct AudioRecList
{
	UInt8	Location;
	UInt32	RecordId;
	UInt32 	RecordingDbId;
	UInt8	Version;
	UInt8	DescLen;
	Char	*Description;
	Boolean SupportsAlarm;
	Boolean Alarm;
	DateTimeType AlarmDateTime;
	struct	AudioRecList *next;	
};

typedef struct{
	UInt8	OBRKey;
	Boolean UseBeep;
	Boolean OBRAfterReset;
	Boolean UseCard;
	Boolean OneButton;
	UInt8	SampleRate;
	UInt8	RecVolume;
	UInt8	PlayVolume;
} LibPreferenceType;

struct PlaybackDataType
{
	Boolean 	stop;
	sndAction	action;
	UInt8		playSource; //0 for internal, 1 for sd
	DmOpenRef	dbRef;
	FileRef		fileRef;
	UInt32		recordId;
	UInt16		frameNum;
	UInt16		frameCount;
	UInt32 		dataLen;	
};

typedef struct
{
	UInt8 version;
	char recDesc[40];
	UInt32 recordId;
	UInt16 frameCount;
	UInt16 sampleRate;
	UInt32 dataLen;	
	Boolean alarm;
	DateTimeType alarmDateTime;
}RecordingType;

typedef struct AudioRecList audio;

static void DataInit(void);
static void UpdateList(void);
static void SaveBuffer(void);
static void LoadPreferences(void);
static void SavePreferences(void);
static Boolean PrefFormHandleEvent(EventType * eventP);
static Boolean OneBPrefFormHandleEvent(EventType * eventP);
static Boolean RecPrefFormHandleEvent(EventType * eventP);
static void ByteSwap32(UInt32 *bytes);
static void ByteSwap16(UInt16 *bytes);
static FileRef CreateWavFile(char * fileName, RecordingType * usrRec);
static void WindowEraseRegion (int l, int t, int w, int h);
static void RefreshList(void);
static void FreeAudio(audio * al);
static audio * GetAudio(int index);
static audio * NewAudio(void);
static WinHandle InitProgressBar(void);
static void UpdateProgressBar(int PercentComplete);
static void KillProgressBar(WinHandle orgWin);
static Boolean DeleteSelection(void);
static void SetControlLabel(const FormType *formP, UInt16 objID, char * text);
static void DrawCenterChars(int top, FontID font, char* text);
static void ShowDetails();
static Boolean DetailsFormHandleEvent(EventType * eventP);
static WinHandle DrawMessage(char* text);
static void KillMessage(WinHandle orgWin);


#endif /* SOUNDREC_H_ */

#ifndef ONEBUTTON_H_
#define ONEBUTTON_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef enum 
{
	oneButtonIdle,
	oneButtonDown,
	oneButtonUp
}oneButtonAction;

typedef enum 
{
	oneButtonFtrAction=1,
	oneButtonFtrTicks,
	oneButtonFtrSndStream,
	oneButtonFtrSndData
}oneButtonFtrVar;

extern void RegisterForNotifications(void);
extern void UnregisterForNotifications(void);
extern void ProcessCmdNotify(MemPtr cmdPBP);
static void SendNilEvent(void);
static void SendKeyEvent(Boolean optionKey);
static void WriteIntFtrValue(void *VarPtr, UInt8 Value);
static void WriteInt32FtrValue(void *VarPtr, UInt32 Value);
static void InitRecording(void);
static void SaveRecording(void);
static Err OpenDatabase(char * DBName, DmOpenRef * DataRef);
static Err CaptureSound(void *userDataP, SndStreamRef stream, void *bufferP, UInt32 frameCount);

#ifdef __cplusplus
}
#endif

#endif
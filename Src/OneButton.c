#include <VFSMgr.h>

#include "OneButton.h"
#include "SoundRec.h"

void ProcessCmdNotify(MemPtr cmdPBP)
{
	SysNotifyParamType *notifyParamsP = (SysNotifyParamType*)cmdPBP;
	if (notifyParamsP->notifyType == 'hede')
	{
		EventType *event;
		event = (EventType*) notifyParamsP->notifyDetailsP;						   					   	
		switch(event->eType)
		{
			case 0x0400://keyDown byteswap		   			
			{	
				void * obrkey;
				void * action = NULL;
				
   				if(!FtrGet(appFileCreator, oneButtonFtrOBRKey, (UInt32*) &obrkey))
   				{
   					UInt16 BSKeyChr = GetChrCode(*((UInt8*) obrkey), true);
   					if(event->data.keyDown.chr == BSKeyChr)
   					{
   						if(!FtrGet(appFileCreator, oneButtonFtrAction, (UInt32*)&action))
						{
							int tmpVal = *((UInt8*) action);
							if(tmpVal == 0)
							{
								//Err err ;	
								void * tckData;
					   			EvtGetEvent(event, 1); //kill the phone app from running
					   			if(FtrGet(appFileCreator, oneButtonFtrTicks, (UInt32*)&tckData))
					   				FtrPtrNew(appFileCreator, oneButtonFtrTicks, 4, &tckData);
								WriteInt32FtrValue(tckData, TimGetTicks());
								WriteIntFtrValue(action, 1);							
								SendNilEvent();
							}else if(tmpVal == 2)
								WriteIntFtrValue(action, 0);
							else if(tmpVal == 3)
								EvtGetEvent(event, 1);	
							else
								EvtGetEvent(event, 1);  
						}
   					}
   				}			   				
				else if(event->data.keyDown.chr == 1026) //if the obr key has not been set, use default
				{
					UInt8	val = 1;			   				
					if(!FtrGet(appFileCreator, oneButtonFtrAction, (UInt32*)&action))
					{
						int tmpVal = *((UInt8*) action);
						if(tmpVal == 0)
						{
							//Err err ;	
							void * tckData;
				   			EvtGetEvent(event, 1); //kill the phone app from running
				   			if(FtrGet(appFileCreator, oneButtonFtrTicks, (UInt32*)&tckData))
				   				FtrPtrNew(appFileCreator, oneButtonFtrTicks, 4, &tckData);
							WriteInt32FtrValue(tckData, TimGetTicks());
							WriteIntFtrValue(action, 1);							
							SendNilEvent();
						}else if(tmpVal == 2)
						{
							WriteIntFtrValue(action, 0);
							EvtGetEvent(event, 1);
						}
						else if(tmpVal == 3)
							EvtGetEvent(event, 1);	
						else
							EvtGetEvent(event, 1);
					}
				}
				break;
		   	}//end keyDown	  			
   			default:	//idle action (nilEvent)
   			{
   				void * action;
				UInt16 KeyBit = keyBitHard1;
				UInt16 KeyChr = 516;
				void * obrkey;			

				if(!FtrGet(appFileCreator, oneButtonFtrOBRKey, (UInt32*) &obrkey))
				{
					UInt16 KeyBits[5] = {keyBitHard1, keyBitHard2, keyBitHard3, keyBitHard4, keyBitRockerCenter};
					KeyBit = KeyBits[*((UInt8*) obrkey)-1];
					KeyChr = GetChrCode(*((UInt8*) obrkey), false);
				}
														   				
   				if(!FtrGet(appFileCreator, oneButtonFtrAction, (UInt32*) &action))
   				{
   					UInt8 tmpVal = *((UInt8*) action);
   					SysTaskDelay(1);
   					if(*((UInt8*) action) == 1)
   					{								
		   				UInt32 keyState = KeyCurrentState();				   		

		   				if(keyState & KeyBit)
		   				{
		   					void * data; 					
							if (!FtrGet(appFileCreator, oneButtonFtrTicks, (UInt32*)&data))
							{
								UInt32 tickCnt = *((UInt32*) data);
								UInt32 tickDiff = 0;
								tickDiff = TimGetTicks() - tickCnt;
								if(tickDiff > 180)
								{
					   				WriteIntFtrValue(action, 3);
					   				SysTaskDelay(1);
									InitRecording();
					   				SendNilEvent();
								}else
									SendNilEvent();										
							}

		   				}else
		   				{									   							   					
			   				//requeue and do not process the next keydown			   				
			   				WriteIntFtrValue(action, 2);
			   				if(keyState & 0xe006)
			   					SendKeyEvent(KeyChr, true);
			   				else
			   					SendKeyEvent(KeyChr, false);
		   				}
		   			}
		   			
		   			if(*((UInt8*) action) == 3) //recording
		   			{
		   				UInt32 keyState = KeyCurrentState();				   		

		   				if(!(keyState & KeyBit)) //done recording
		   				{
		   					Err error;
		   					SndStreamRef sndRef;
		   					struct PlaybackDataType sndData;
		   					void * ftrSndData;
		   					void * ftrSndRef;

							LibPreferenceType prefs;
							UInt16 prefSize;
							Boolean 		useBeep = true;
		
							prefSize = sizeof(LibPreferenceType);
							if(PrefGetAppPreferences(libCreatorID, libPrefID, &prefs, &prefSize, true) != noPreferenceFound)
							{
								useBeep = prefs.UseBeep;
							}		   					
		   					
							FtrGet(appFileCreator, oneButtonFtrSndData, (UInt32*) &ftrSndData);
							MemMove(&sndData, ftrSndData, sizeof(sndData));
							FtrGet(appFileCreator, oneButtonFtrSndStream, (UInt32*) &ftrSndRef);
							MemMove(&sndRef, ftrSndRef, sizeof(SndStreamRef));

							sndData.stop = true;
							sndData.action = none;
							
							DmWrite(ftrSndData, 0, &sndData, sizeof(sndData));
																					
		   					error = SndStreamStop(sndRef);		
							error = SndStreamDelete(sndRef);
							if(useBeep) SndPlaySystemSound(sndConfirmation);
							FtrPtrFree(appFileCreator, oneButtonFtrSndStream);
							SysTaskDelay(1);
			   				SaveRecording();
		   					WriteIntFtrValue(action, 0);
		   					SendNilEvent();
		   				}
		   			}
   				}
   				break;
   			} //end idle		   			
   		} //end switch
	} //end hede check
}

void RegisterForNotifications(void)
{
	UInt16 cardNo;
	LocalID dbID;
	void * action = 0;
	
  	if (0 != SysCurAppDatabase(&cardNo, &dbID))
		return;
  
	UnregisterForNotifications(); //this is safe I think??!?!
  
	if(FtrPtrNew(appFileCreator, oneButtonFtrAction, 1,&action))
		return;
	
	if(FtrGet(appFileCreator, oneButtonFtrAction, (UInt32*) &action))
		return;
		
	WriteIntFtrValue(action, 0);

	SysNotifyRegister(cardNo, dbID, 'hede', NULL, sysNotifyNormalPriority, 0);  
}

extern void UnregisterForNotifications(void)
{
	UInt16 cardNo;
	LocalID dbID;
	
  	if (0 != SysCurAppDatabase(&cardNo, &dbID))
		return;
	//clean up the ftr vars too!!!!
	
	SysNotifyUnregister(cardNo, dbID, 'hede', sysNotifyNormalPriority);	
}

static void WriteIntFtrValue(void *VarPtr, UInt8 Value)
{
	DmWrite(VarPtr, 0, &Value, 1);
}

static void WriteInt32FtrValue(void *VarPtr, UInt32 Value)
{
	DmWrite(VarPtr, 0, &Value, 4);
}

static void SendKeyEvent(WChar keyChr, Boolean optionKey)
{
	EventType event;	
	event.eType = 0x0004;
	event.data.keyDown.chr = keyChr;
	event.data.keyDown.keyCode = 0;	
	event.data.keyDown.modifiers = 12;	
	EvtAddEventToQueue(&event);	
}

static void SendNilEvent(void)
{
	EventType newEvent;
	newEvent.eType = 0x0000;
	newEvent.data.keyDown.chr = 0;
	newEvent.data.keyDown.keyCode = 0;
	newEvent.data.keyDown.modifiers = 0;
	EvtAddEventToQueue(&newEvent);
}

static void InitRecording(void)
{	
	Err error;
	DmOpenRef	soundDb; 	
	SndStreamRef sndRef = sysInvalidRefNum;
	struct PlaybackDataType sndData;	
	void * ftrSndData = NULL; //MemPtrNew(sizeof(struct PlaybackDataType));
	void * ftrSndRef = NULL; //MemPtrNew(sizeof(SndStreamRef));

	//clean memory
	MemSet(&sndData, sizeof(struct PlaybackDataType), 0);
	
	//open the database
	error = OpenDatabase(sndDBName, &soundDb);		

	if(!error)
	{				
		LibPreferenceType prefs;
		UInt16 prefSize;
		Boolean 		useCard = false;
		UInt16 			soundRate = 20000;
		UInt8			recVol = 3;
		Boolean 		useBeep = true;
		
		prefSize = sizeof(LibPreferenceType);
		if(PrefGetAppPreferences(libCreatorID, libPrefID, &prefs, &prefSize, true) != noPreferenceFound)
		{
			useCard = prefs.UseCard;
			switch(prefs.SampleRate)
			{
				case 44:
					soundRate = 44100;
				case 22:
					soundRate = 22050;
				case 11:
					soundRate = 11025;
				default:
					soundRate = prefs.SampleRate * 1000;
			}
			recVol = prefs.RecVolume;
			useBeep = prefs.UseBeep;
		}		
		
		sndData.stop = false;
		sndData.dbRef = soundDb;	
		sndData.frameNum = 0;
		sndData.frameCount = 0;
		sndData.recordId = -1;
		sndData.playSource = 0;
		sndData.fileRef = 0;
		sndData.action = record;			
		
		if(FtrGet(appFileCreator, oneButtonFtrSndData, (UInt32*) ftrSndData) != 0)			
			error = FtrPtrNew(appFileCreator, oneButtonFtrSndData, sizeof(struct PlaybackDataType), &ftrSndData);					
		DmWrite(ftrSndData, 0, &sndData, sizeof(sndData)); 

		//create snd stream
		error = SndStreamCreate(&sndRef, sndInput, soundRate, sndInt16Little, sndMono, CaptureSound, ftrSndData, (UInt32) (8000), false);	
		
		//save snd stream
		if(FtrGet(appFileCreator, oneButtonFtrSndStream, (UInt32*) ftrSndRef) != 0)
			FtrPtrNew(appFileCreator, oneButtonFtrSndStream, sizeof(SndStreamRef), &ftrSndRef);			
		DmWrite(ftrSndRef, 0, &sndRef, sizeof(SndStreamRef)); 		
			
		if(!error)
		{			
			if(useBeep) SndPlaySystemSound(sndConfirmation);
			SndStreamSetVolume (sndRef, recVol * 500);
			SndStreamStart(sndRef);			
		}
	}
}

static void SaveRecording(void)
{
	UInt16 index = dmMaxRecordIndex;
	MemHandle h;
	char buffer[5];
	char recDesc1[40];	
	DateTimeType *dateTimeP = NULL;
	Err error;
	DmOpenRef	lrecordingDb; 
	struct PlaybackDataType * sndData;
	char month[12][4] = {"Jan\0", "Feb\0", "Mar\0", "Apr\0", "May\0", "Jun\0", "Jul\0", "Aug\0", "Sep\0", "Oct\0", "Nov\0", "Dec\0"};
	UInt8 	sndVersion 	= 103;
	
	if(FtrGet(appFileCreator, oneButtonFtrSndData, (UInt32*) &sndData) == 0)
	{
		error = OpenDatabase(recDBName, &lrecordingDb);

		if(!error) //if there is an error you need to clean up the snd database
		{
			dateTimeP = MemPtrNew(sizeof(DateTimeType));
			TimSecondsToDateTime (TimGetSeconds(), dateTimeP);

			StrCopy(recDesc1, month[dateTimeP->month - 1]);
			StrCat(recDesc1," ");
			StrIToA(buffer, dateTimeP->day);
			StrCat(recDesc1, buffer);
			StrCat(recDesc1,", ");
			StrIToA(buffer, dateTimeP->year);
			StrCat(recDesc1, buffer);
			StrCat(recDesc1," ");

			StrIToA(buffer,dateTimeP->hour);
			StrCat(recDesc1, buffer);
			StrCat(recDesc1, ":");
			StrIToA(buffer,dateTimeP->minute);
			StrCat(recDesc1, buffer);
			StrCat(recDesc1, ".");
			StrIToA(buffer,dateTimeP->second);
			StrCat(recDesc1, buffer);		
			MemPtrFree(dateTimeP);	


			h = DmNewRecord(lrecordingDb, &index,  54);
			if(h)
			{
				MemPtr 				recPtr;
				LibPreferenceType 	prefs;
				UInt16 				prefSize;
				UInt16 				soundRate = 20000;

				prefSize = sizeof(LibPreferenceType);
				if(PrefGetAppPreferences(libCreatorID, libPrefID, &prefs, &prefSize, true) != noPreferenceFound)
				{
					switch(prefs.SampleRate)
					{
						case 44:
							soundRate = 44100;
						case 22:
							soundRate = 22050;
						case 11:
							soundRate = 11025;
						default:
							soundRate = prefs.SampleRate * 1000;
					}
				}				

				recPtr = MemHandleLock(h);
				DmWrite(recPtr, 0, &sndVersion, 1);							
				DmWrite(recPtr, 1 , &recDesc1[0], 40);
				DmWrite(recPtr, 42, &sndData->recordId, 4);
				DmWrite(recPtr, 46, &sndData->frameCount, 2);
				DmWrite(recPtr, 48, &soundRate, 2);
				DmWrite(recPtr, 50, &sndData->dataLen, 4); 
				MemHandleUnlock(h);
				DmReleaseRecord( lrecordingDb, index, true );
			 	DmCloseDatabase( lrecordingDb );
			}
		}
		DmCloseDatabase( sndData->dbRef );
	}
	FtrPtrFree(appFileCreator, oneButtonFtrSndData);	
}

static Err OpenDatabase(char * DBName, DmOpenRef * DataRef)
{
	Err error = errNone;
	LocalID dbId;
	//static UInt16	CardId 			= 0;
	
	dbId = DmFindDatabase (0, DBName);
	
	if(dbId)
	{
		*DataRef = DmOpenDatabase(0, dbId, dmModeReadWrite);
		if(!(*DataRef))
		{
			error = DmCreateDatabase(0, DBName, libCreatorID, libDBType, false);
			if(!error)
			{
				*DataRef = DmOpenDatabase(0, dbId, dmModeReadWrite);
			}
		}
	}else
	{
		error = DmCreateDatabase(0, DBName, libCreatorID, libDBType, false);
		if(!error)
		{
			dbId = DmFindDatabase (0, DBName);
			if(dbId)
			{
				*DataRef = DmOpenDatabase(0, dbId, dmModeReadWrite);
			}else
				return dmErrCantFind;
	
		}
	}
	
	return error;
}

static Err CaptureSound(void *userDataP, SndStreamRef stream, void *bufferP, UInt32 frameCount)
{
	UInt32 freeBytes = 0;
	struct PlaybackDataType dataBlockP;
//	struct PlaybackDataType dataBlockP2;
	//void * ftrSndData;
	//FtrGet(appFileCreator, oneButtonFtrSndData, (UInt32*) &ftrSndData);
	//MemMove(&dataBlockP, ftrSndData, sizeof(dataBlockP));
	MemMove(&dataBlockP, userDataP, sizeof(dataBlockP));
	
	MemCardInfo (0,0,0,0,0,0,0, &freeBytes);

	if(!dataBlockP.stop && dataBlockP.action != busy && (freeBytes > 3000000 || dataBlockP.playSource > 0))
	{
		dataBlockP.action = busy;
		//if(dataBlockP->playSource == 0)
		{								
			if(dataBlockP.dbRef)
			{
				UInt16 index = dmMaxRecordIndex;
				MemHandle h;
				
				h = DmNewRecord(dataBlockP.dbRef, &index,  frameCount * sizeof(Int16)+2);
		       	if(h)
		       	{
					MemPtr  recPtr;   
					UInt16 	theSize = 0;            
					// Copy the bitmap data into the new record
					recPtr = MemHandleLock( h );    		   				    		 
					theSize = frameCount * sizeof(Int16);
					DmWrite(recPtr, 0, &theSize, 2);
			   		DmWrite(recPtr, 2, bufferP, frameCount * sizeof(Int16));
					MemHandleUnlock( h );
					if(dataBlockP.recordId == -1) 
						DmRecordInfo (dataBlockP.dbRef, index, NULL, &dataBlockP.recordId, NULL);
					DmReleaseRecord (dataBlockP.dbRef, index, true);
					dataBlockP.dataLen += frameCount * sizeof(Int16);
					dataBlockP.frameCount++;
					dataBlockP.frameNum++;
		       	}
		       	else
		       	{
		       		dataBlockP.stop = true;      	
					dataBlockP.action = none;
	       		}	       		     	       		           		     
	       		       			
			}else
			{
				dataBlockP.stop = true;  		
				dataBlockP.action = none;
			}
		}
		dataBlockP.action = record;				
	}	
	else
	{
		dataBlockP.action = none;
		dataBlockP.stop = true;
	}		
	MemSet(bufferP, MemPtrSize(bufferP), 0);
	DmWrite(userDataP, 0, &dataBlockP, sizeof(dataBlockP));
	return errNone;
}

static void ByteSwap16(UInt16 *bytes)
{
	UInt8 t;
	MemMove(&t, bytes, 1);
	MemMove((void *) ((UInt8 *) bytes), (void *) ((UInt8 *) bytes + 1), 1);
	MemMove((void *) ((UInt8 *) bytes + 1), &t, 1);
}

static UInt16 GetChrCode(int KeyNum, Boolean ByteSwap)
{
	//support for 4 hard keys and center
	UInt16 ChrCodes[5] = {516,517,518,519,310};
	UInt16 BSChrCodes[5] = {1026,1282,1538,1794,13825};
	
	if(ByteSwap)
		return BSChrCodes[KeyNum-1];
	else
		return ChrCodes[KeyNum-1];
	
}
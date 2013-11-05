/*
 * SoundRec.c
 *
 * Copyright (c) 2004 Ryan Rife.
 * All rights reserved.
 */
 
#include <PalmOS.h>
#include <PalmOSGlue.h>
#include <VFSMgr.h>
//#include <Hs.h> //for Chiru
#include <68k/Hs.h> //for Ryan

#include "SoundRec.h"
#include "OneButton.h"
#include "SoundRec_Rsc.h"

/* Define the minimum OS version we support */
#define ourMinVersion    sysMakeROMVersion(3,0,0,sysROMStageDevelopment,0)
#define kPalmOS20Version sysMakeROMVersion(2,0,0,sysROMStageDevelopment,0)

static DmOpenRef	soundDb; 
static DmOpenRef	recordingDb; 
static SndStreamRef	gSndStreamRef = sysInvalidRefNum;

static Boolean		recording = false;
static Boolean 		playing = false;
static UInt16		gVolRefNum = sysInvalidRefNum;

static char months[12][4] = {"Jan\0", "Feb\0", "Mar\0", "Apr\0", "May\0", "Jun\0", "Jul\0", "Aug\0", "Sep\0", "Oct\0", "Nov\0", "Dec\0"};	

static struct 		PlaybackDataType sndData;
static Boolean 		useCard = false;
static Boolean 		oneButton = false;
static UInt16 		soundRate = 24000;
static UInt8		recVol = 3;
static UInt8		playVol = 3;
static UInt16		audioListCount = 0;
static UInt16		lastHighlight = 0;
static audio 		*audioList;
static WinHandle 	progWin, savedWin; //this is for the progress bar
static FormType 	*mainForm;
static Boolean          timeOn = false;
static UInt32           startTime = 0;//this is for the timer
static UInt32 hr = 0;
static UInt32 min = 0;
static UInt32 sec = 0;
static char recDesc[40]="";


static void addFldString(FormPtr form, UInt16 objectID, char *string)
{
  char *str;
  FieldType *fieldPtr;  
  UInt32 size;
  MemHandle textH;
  MemHandle oldTextH;
  
  fieldPtr = FrmGetObjectPtr(form, FrmGetObjectIndex(form, objectID));
  size = FldGetMaxChars(fieldPtr);
  oldTextH=FldGetTextHandle(fieldPtr);
  textH=MemHandleNew(size);
  str = MemHandleLock(textH);
  
  str = StrCopy(str, string);
  
  MemHandleUnlock(textH);
  FldSetTextHandle(fieldPtr, textH);  
  FldDrawField(fieldPtr);
  if(oldTextH)
    MemHandleFree(oldTextH);    
}

static void dispTimer()
{

  char hrStr[3]="00\0";
  char minStr[5]=": 00\0";
  char secStr[5]=": 00\0";

  UInt32 currentTime;
  FormPtr form;

  form = FrmGetActiveForm();

  if(startTime == 0)
    startTime = TimGetSeconds();
  
  {
    currentTime = TimGetSeconds();
    if(startTime != currentTime){
      sec = currentTime-startTime;
      if(sec < 60){
	if(sec < 10)
	  StrIToA(&secStr[3], sec);
	else
	  StrIToA(&secStr[2], sec);
	addFldString(form, fldTimeSec,secStr);
      }
      else{
	sec = sec-60;
	if(sec < 10)
	  StrIToA(&secStr[3], sec);
	else
	  StrIToA(&secStr[2], sec);
	addFldString(form, fldTimeSec,secStr);

	min++;
	if(min < 60){
	  if(min < 10)
	    StrIToA(&minStr[3], min);
	  else
	    StrIToA(&minStr[2], min);
	  addFldString(form, fldTimeMin,minStr);
	}
	else{
	  min = min-60;
	  if(min < 10)
	    StrIToA(&minStr[3], min);
	  else
	    StrIToA(&minStr[2], min);
	  addFldString(form, fldTimeMin,minStr);
	  
	  hr++;
	  if(hr < 10)
	    StrIToA(&hrStr[1], hr);
	  else
	    StrIToA(&hrStr[0], hr);
	  addFldString(form, fldTimeHr,hrStr);
	}
	startTime=currentTime;
      }
    }
  }
}


static void FrmSetObjectFocusMode(FormPtr formP)
{
  FrmNavStateFlagsType		navStateFlags;
  Err				navErr;


  // Get the current nav state flags for the form
  navErr = FrmGetNavState (formP, &navStateFlags);

  if (navErr != errNone)
	{
	  ErrNonFatalDisplay ("Error getting nav state");
	  return;
	}


  // we're turning on object focus mode, set the object focus
  //  mode flag in the state flags field we just retrieved.
  
  navStateFlags |= kFrmNavStateFlagsObjectFocusMode; 

  // Set the new nav state flags
  navErr = FrmSetNavState (formP, navStateFlags);
  ErrNonFatalDisplayIf (navErr, "Error setting nav state");
}

static void
CustomNavOrder (FormType* formP, UInt16 buttonID)
{
  FrmNavHeaderType		navHeader;
  FrmNavOrderEntryType*	navOrderP = NULL;
  UInt16				numNavObjects;
  Err					navErr;


  //---------------------------------------------------------------
  // Allocate an array for storing the current nav order
  //---------------------------------------------------------------
  numNavObjects = FrmCountObjectsInNavOrder (formP);

  navOrderP = MemPtrNew (sizeof (*navOrderP) * numNavObjects);

  if (!navOrderP)
	{
	  ErrNonFatalDisplay ("Error allocating nav order array");
	  goto Exit;
	}

  MemSet (navOrderP, sizeof (*navOrderP) * numNavObjects, 0);


  //---------------------------------------------------------------
  // Get the current nav order
  //---------------------------------------------------------------
  navErr = FrmGetNavOrder (formP, &navHeader, navOrderP, 
						   &numNavObjects);

  if (navErr != errNone)
	{
	  ErrNonFatalDisplay ("Error getting nav order");
	  goto Exit;
	}


  //---------------------------------------------------------------
  // Change the current nav order
  //---------------------------------------------------------------
  // make nav focus on record button first
  navHeader.initialObjectIDHint = buttonID;

  //---------------------------------------------------------------
  // Set the customized nav order
  //---------------------------------------------------------------
  navErr = FrmSetNavOrder (formP, &navHeader, navOrderP);

  if (navErr != errNone)
	{ 
	  ErrNonFatalDisplayIf (navErr, "Error setting nav order");
	  goto Exit;
	}

Exit:

  if (navOrderP)
	{
	  MemPtrFree (navOrderP);
	}
}

static void *GetFormObjectPtr(FormPtr frmP, Int index)
{
	Word fldIndex = FrmGetObjectIndex(frmP, index);
	return FrmGetObjectPtr(frmP, fldIndex);
}


static void * GetObjectPtr(UInt16 objectID)
{
	FormType * frmP;

	frmP = FrmGetActiveForm();
	return FrmGetObjectPtr(frmP, FrmGetObjectIndex(frmP, objectID));
}


static void MainListDraw(Int16 itemNum, RectangleType *bounds, Char **itemsText)
{	
	MemHandle locBitmapH;
	BitmapType *bitmap;
	
	if(audioListCount > 0)
	{
		audio * cAudio = GetAudio(itemNum);

		if(cAudio != NULL)
		{
			switch(cAudio->Location)
			{
				case 0:
					locBitmapH = DmGetResource(bitmapRsc, bmpInternal);
					break;
				case 1:
					locBitmapH = DmGetResource(bitmapRsc, bmpSdCard);
					break;
			}
			if(locBitmapH)
			{
				bitmap = (BitmapPtr)MemHandleLock(locBitmapH);			
				WinDrawBitmap(bitmap, bounds->topLeft.x, bounds->topLeft.y);
			}
			WinDrawChars(cAudio->Description, StrLen(cAudio->Description), bounds->topLeft.x+14, bounds->topLeft.y);
		}
		//WinDrawChars("hello", 5, bounds->topLeft.x, bounds->topLeft.y);
		
    	MemHandleUnlock(locBitmapH);
    	DmReleaseResource(locBitmapH);
	}	
}

static Err CaptureSound(void *userDataP, SndStreamRef stream, void *bufferP, UInt32 frameCount)
{
	UInt32 freeBytes = 0;
	struct PlaybackDataType *dataBlockP = (struct PlaybackDataType *)userDataP;

	MemCardInfo (0,0,0,0,0,0,0, &freeBytes);

	if(!dataBlockP->stop && (freeBytes > 3000000 || dataBlockP->playSource == 0))
	{
		if(dataBlockP->playSource == 0)
		{								
			if(dataBlockP->dbRef)
			{
				UInt16 index = dmMaxRecordIndex;
				MemHandle h;
				
				h = DmNewRecord(dataBlockP->dbRef, &index,  frameCount * sizeof(Int16)+2);
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
					if(dataBlockP->recordId == -1) DmRecordInfo (dataBlockP->dbRef, index, NULL, &dataBlockP->recordId, NULL);
					DmReleaseRecord (dataBlockP->dbRef, index, true);
					dataBlockP->dataLen += frameCount * sizeof(Int16);
					dataBlockP->frameCount++;
					dataBlockP->frameNum++;
		       	}
		       	else
		       	{
		       		dataBlockP->stop = true;      	
					dataBlockP->action = none;
	       		}	       		     	       		           		     
	       		       			
			}else
			{
				dataBlockP->stop = true;  		
				dataBlockP->action = none;
			}
		}else
		{
			if(dataBlockP->fileRef != NULL)
			{
				UInt32 bytesLeft = frameCount * sizeof(Int16);
				UInt32 offset = 0;
				UInt32 bytesRead = 0;
				while(bytesLeft > 0)
				{
					VFSFileWrite(dataBlockP->fileRef, bytesLeft, (UInt8 *) bufferP + offset, &bytesRead);
					bytesLeft -= bytesRead;
					offset += bytesRead;
				}
				//store & write the file size
				dataBlockP->dataLen += frameCount * sizeof(Int16);
				dataBlockP->frameCount++;
				dataBlockP->frameNum++;				
				
			}else
			{
				dataBlockP->stop = true;  		
				dataBlockP->action = none;
			}	
		}				
	}	
	else
	{
		dataBlockP->action = none;
		dataBlockP->stop = true;
	}		
	MemSet(bufferP, MemPtrSize(bufferP), 0);
	return errNone;
}

static Err PlaySound(void *userDataP, SndStreamRef stream, void *bufferP, UInt32 frameCount)
{
	Err err = errNone;
	UInt32 theSize;
	void * theRecPtr;
	MemHandle recordH;
	UInt32 dataSizeP = 0;
	UInt32 headerSizeP = 0;
	UInt16 numRecs = 0;
	UInt16 recIndex = 0;
	struct PlaybackDataType *dataBlockP = (struct PlaybackDataType *)userDataP;
	
	if(!dataBlockP->stop)
	{				

		if(dataBlockP->playSource == 0)
		{
			err = DmFindRecordByID( dataBlockP->dbRef, dataBlockP->recordId, &recIndex);
			if(!err && (dataBlockP->frameNum < dataBlockP->frameCount))
			{					
				recordH = DmQueryRecord(dataBlockP->dbRef, recIndex + dataBlockP->frameNum);
				if(recordH)
				{					
				    theRecPtr = MemHandleLock( recordH );
				    // Copy the data from the record into the bitmap pointer global
				    theSize = MemPtrSize(theRecPtr) - 2;
				    MemMove(bufferP, (void *) ((UInt8 *) theRecPtr + 2), theSize);
				    MemHandleUnlock( recordH );
				   // DmReleaseRecord( soundDb, recIndex + dataBlockP->frameNum, false);
				    dataBlockP->frameNum++;			
				}else
				{
					dataBlockP->stop = true;
					dataBlockP->action = none;
				}
			}else
			{
				dataBlockP->stop = true;
				dataBlockP->action = none;
			}
		}else
		{
			if(dataBlockP->fileRef != NULL)
			{
				if(!VFSFileEOF(dataBlockP->fileRef))
				{
					UInt32 bufferOffset = 0;
					UInt16 bytesLeft = frameCount * sizeof(UInt16);
					UInt32 bytesRead = 0;
					while(bytesLeft > 0 && !VFSFileEOF(dataBlockP->fileRef))
					{
						err = VFSFileRead(dataBlockP->fileRef, bytesLeft, (UInt16 *) bufferP + bufferOffset, &bytesRead);
						bytesLeft -= bytesRead;
						bufferOffset += bytesRead;
						if(err)
						{
							dataBlockP->stop = true;
							dataBlockP->action = none;
							break;
						}
					}	
				
				}else
				{
					dataBlockP->stop = true;
					dataBlockP->action = none;
				}				
			}else
			{
				dataBlockP->stop = true;
				dataBlockP->action = none;
			}			
		}
		
	}
	else
	{
		dataBlockP->action = none;
		dataBlockP->stop = true;
	}
	return errNone;
}


static void MainFormInit(FormType *frmP)
{
	ListType *list = GetObjectPtr(lstRecordings);
	LstSetDrawFunction(list, MainListDraw);
	UpdateList();
	//FrmSetFocus(frmP, FrmGetObjectIndex (frmP, btnRecord));
	CustomNavOrder(frmP, btnRecord);
	FrmSetObjectFocusMode(frmP);

}



static Boolean MainFormDoCommand(UInt16 command)
{
	Boolean handled = false;

	switch (command)
	{
		case OptionsAboutSoundRec:
		{
			FormType * frmP;

			/* Clear the menu status from the display */
			MenuEraseStatus(0);

			/* Display the About Box. */
			frmP = FrmInitForm (AboutForm);
			FrmDoDialog (frmP);                    
			FrmDeleteForm (frmP);

			handled = true;
			break;
		}
		case mnuDetails:
		{
			UInt16 selItem = 0;
			UInt16 answer = 0;
			Boolean stop = false;
			ListType *list = GetObjectPtr(lstRecordings);

			if(LstGetSelection(list) >= 0 && audioList != NULL)
			{		
				FormType * frmP;
				audio * cAudio;
				MemHandle descText;

				/* Clear the menu status from the display */
				MenuEraseStatus(0);
				frmP = FrmInitForm (DetailForm);
				descText = MemHandleNew(41);

				
				cAudio = GetAudio(LstGetSelection(list));
				

				StrCopy(MemHandleLock(descText), cAudio->Description);
				MemHandleUnlock(descText);
				FldSetText(GetFormObjectPtr(frmP, fldTitle), descText, 0, 40);
				
				while(!stop)
				{	
					answer = FrmDoDialog (frmP); 						
					if(answer == btnDetailOk)
					{
						if(cAudio->Location == 0)
						{
							if(StrLen(FldGetTextPtr (GetFormObjectPtr(frmP, fldTitle))) > 0)
							{
								if(cAudio->RecordingDbId > 0)
								{
									UInt16 recIndex = 0;
									MemHandle recH;	
									MemPtr recPtr;	
									RecordingType * usrRec;							
									usrRec = MemPtrNew(sizeof(RecordingType));
									DmFindRecordByID(recordingDb, cAudio->RecordingDbId, &recIndex);
									recH = DmGetRecord(recordingDb, recIndex);
									//usrRec = (RecordingType *) MemHandleLock(recH);
									//StrCopy(&usrRec->recDesc[0], "Hello World");
									recPtr = MemHandleLock(recH);
									MemMove(usrRec, recPtr, sizeof(RecordingType));
									StrCopy(&usrRec->recDesc[0], FldGetTextPtr(GetFormObjectPtr(frmP, fldTitle)));	
									DmWrite(recPtr, 0, usrRec, sizeof(RecordingType));
									MemHandleUnlock(recH);
									DmReleaseRecord(recordingDb, recIndex, true);
									MemPtrFree(usrRec);
								}
								
								stop = true;
							}
							else						
								FrmAlert(DescAlert);
						}
						
						if(cAudio->Location > 0 && gVolRefNum != sysInvalidRefNum)
						{
							Err err;
							char oldFile[255];
							StrCopy(oldFile, SDAudioDir);
							StrCat(oldFile, "/");
							StrCat(oldFile, cAudio->Description);
							err = VFSFileRename(gVolRefNum, oldFile, FldGetTextPtr(GetFormObjectPtr(frmP, fldTitle)));
							if(!err)							
								stop = true;
							else
							{
								FrmAlert(FileAlert);
							}
						}						
					}
					
					
					if(answer == btnDetailCancel)							
						stop = true;
						
					if(answer == btnDetailDel)
					{
						if(DeleteSelection())
						{
							UpdateList();
							stop = true;
						}
					}
				}
				//MemHandleFree (descText);                   
				FrmDeleteForm (frmP);
			}
			UpdateList();
			handled = true;
			break;			
		}
		case mnuExport:
		{
			UInt16 selItem = 0;
			ListType *list = GetObjectPtr(lstRecordings);
			char buffer[100];

			if(LstGetSelection(list) >= 0 && audioList != NULL)
			{
				audio * cAudio;
				cAudio = GetAudio(LstGetSelection(list));
				
				if(cAudio->Location == 0)
				{
					MemHandle h;
					int i;
					h = DmQueryRecord(recordingDb, LstGetSelection(list));
					if(h)
					{
						FileRef fileRef;
						RecordingType * usrRec;
						usrRec = (RecordingType *) MemHandleLock(h);
						
						StrCopy(buffer,SDAudioDir);
						StrCat(buffer,"/");
						StrCat(buffer, usrRec->recDesc);					
						for(i=0;i<StrLen(buffer);i++)
						{
							if(StrNCompare(&buffer[i],":",1) == 0) buffer[i]= '_';
							if(StrNCompare(&buffer[i],".",1) == 0) buffer[i]= '_';
							if(StrNCompare(&buffer[i],",",1) == 0) buffer[i]= '_';
							if(StrNCompare(&buffer[i]," ",1) == 0) buffer[i]= '_';
						}
						StrCat(buffer, ".wav");
											
						fileRef = CreateWavFile(buffer, usrRec);
						if(fileRef != NULL) VFSFileClose(fileRef);
						MemHandleUnlock(h);																								
					}
				}
			}
			UpdateList();
			break;
		}
		case mnuPrefs:
			FrmPopupForm(PrefForm);
			break;
		case mnuDelete:
		{
			DeleteSelection();
			UpdateList();
			break;
		}

	}

	return handled;
}



static Boolean MainFormHandleEvent(EventType * eventP)
{
	Boolean handled = false;
	FormType * frmP;
	frmP = FrmGetActiveForm();

	switch (eventP->eType) 
	{
	case nilEvent:
	  
	  if(timeOn){
	    dispTimer();
	  }					
		

			if(sndData.action == none && (recording || playing))
			{
				UInt16 objIndex;
				
				SndStreamStop(gSndStreamRef);		
				SndStreamDelete(gSndStreamRef);				
				
				objIndex = FrmGetObjectIndex(frmP, btnStop);
				FrmHideObject(frmP, objIndex);	
				
				//chiru - hide the file name 
				objIndex = FrmGetObjectIndex(frmP, fldFileName);
				FrmHideObject(frmP, objIndex);	
				
				//chiru - hide the file destination
				objIndex = FrmGetObjectIndex(frmP, fldFileDest);
				FrmHideObject(frmP, objIndex);	

				//chiru - hide the time descriptions
				objIndex = FrmGetObjectIndex(frmP, fldTimeDesc);
				FrmHideObject(frmP, objIndex);	
				objIndex = FrmGetObjectIndex(frmP, fldTimeHr);
				FrmHideObject(frmP, objIndex);	
				objIndex = FrmGetObjectIndex(frmP, fldTimeMin);
				FrmHideObject(frmP, objIndex);	
				objIndex = FrmGetObjectIndex(frmP, fldTimeSec);
				FrmHideObject(frmP, objIndex);	

				//chiru - show the list of the recorded files
				objIndex = FrmGetObjectIndex(frmP, lstRecordings);
				FrmShowObject(frmP, objIndex);
		
				objIndex = FrmGetObjectIndex(frmP, btnPlay);
				FrmShowObject(frmP, objIndex);
			
				objIndex = FrmGetObjectIndex(frmP, btnRecord);
				FrmShowObject(frmP, objIndex);
				
				CustomNavOrder(frmP, btnRecord);
				FrmSetObjectFocusMode(frmP);


				if(playing)
					if(sndData.playSource > 0 && sndData.fileRef != NULL)
						VFSFileClose(sndData.fileRef);
					
				SaveBuffer();								
				UpdateList();
			}
			handled = true;
			break;
			
	case menuEvent:
			return MainFormDoCommand(eventP->data.menu.itemID);

	case frmOpenEvent:
			FrmDrawForm(frmP);
			MainFormInit(frmP);
			handled = true;
			break;
            
	case frmUpdateEvent:
			/* 
			 * To do any custom drawing here, first call
			 * FrmDrawForm(), then do your drawing, and
			 * then set handled to true. 
			 */
			break;
			
	case lstSelectEvent:
		{
			switch(eventP->data.lstSelect.listID)
			{
				case lstRecordings:
				{
					ListType *list = GetObjectPtr(lstRecordings);					
					lastHighlight = LstGetSelection(list);				
					break;
				}			
			}
			break;
		}		
	case ctlSelectEvent:
		{
			UInt16 objIndex;
	
			switch(eventP->data.ctlSelect.controlID)
			{

				case btnPlay:
				{
					Err error;

					UInt16 selItem = 0;
					ListType *list = GetObjectPtr(lstRecordings);

					if(LstGetSelection(list) >= 0 && audioList != NULL)
					{
						audio * cAudio;
						cAudio = GetAudio(LstGetSelection(list));
						
						if(cAudio->Location == 0)
						{
							MemHandle h;
							h = DmQueryRecord(recordingDb, LstGetSelection(list));
							if(h)
							{
								RecordingType * usrRec;
								usrRec = (RecordingType *) MemHandleLock(h);
								sndData.recordId = usrRec->recordId;
								sndData.frameCount = usrRec->frameCount;
								MemHandleUnlock(h);
								
								sndData.stop = false;
								sndData.dbRef = soundDb;	
								sndData.frameNum = 0;
								sndData.playSource = 0;
								sndData.action = play;
								sndData.dataLen = 0;
								error = SndStreamCreate(&gSndStreamRef, sndOutput, usrRec->sampleRate, sndInt16Little, sndMono, PlaySound, &sndData, (UInt32) (2000), false);
								if(!error)
								{
									SndStreamSetVolume (gSndStreamRef, (UInt16) (playVol * 500));
									objIndex = FrmGetObjectIndex(frmP, btnPlay);
									FrmHideObject(frmP, objIndex);
							
									objIndex = FrmGetObjectIndex(frmP, btnRecord);
									FrmHideObject(frmP, objIndex);
									
									objIndex = FrmGetObjectIndex(frmP, btnStop);
									FrmShowObject(frmP, objIndex);						
														
									SndStreamStart(gSndStreamRef);
									//FrmSetFocus(frmP, FrmGetObjectIndex (frmP, btnStop));
									CustomNavOrder(frmP, btnStop);
									FrmSetObjectFocusMode(frmP);
									playing = true;																			
								}
							}
						}
						if(cAudio->Location > 0)
						{
							if(gVolRefNum != sysInvalidRefNum)
							{
								FileRef dirRef;							
								char *fileName = MemPtrNew(256); 
								char filePath[270];
								Err err;

								err = VFSFileOpen(gVolRefNum, SDAudioDir, vfsModeRead, &dirRef);
								if(!err)
								{
									StrCopy(filePath, SDAudioDir);
									StrCat(filePath, "/");
									StrCat(filePath, cAudio->Description);
									err = VFSFileOpen(gVolRefNum, filePath, vfsModeRead, &sndData.fileRef);
									if(!err) 
									{						
										WavType * usrWav;								
										UInt32 bytesRead = 0;
										UInt32 bufferOffset = 0;
										UInt16 bytesLeft = sizeof(WavType);
										usrWav = MemPtrNew(sizeof(WavType));
										while(bytesLeft > 0 && !VFSFileEOF(sndData.fileRef))
										{
											err = VFSFileRead(sndData.fileRef, bytesLeft, (UInt16 *) usrWav + bufferOffset, &bytesRead);
											bytesLeft -= bytesRead;
											bufferOffset += bytesRead;
											if(err)
												break;
										}																				
										if(!err && bytesRead == sizeof(WavType)) 
										{
											sndData.stop = false;
											sndData.dbRef = soundDb;											
											sndData.playSource = 1;												
											sndData.frameNum = 0;
											sndData.action = play;
											sndData.dataLen = 0;
											
											ByteSwap32(&usrWav->sampleRate);
											
											error = SndStreamCreate(&gSndStreamRef, sndOutput, usrWav->sampleRate, sndInt16Little, sndMono, PlaySound, &sndData, (UInt32) (2000), false);
											if(!error)
											{
												SndStreamSetVolume (gSndStreamRef, (UInt16) (playVol * 500));
												objIndex = FrmGetObjectIndex(frmP, btnPlay);
												FrmHideObject(frmP, objIndex);
										
												objIndex = FrmGetObjectIndex(frmP, btnRecord);
												FrmHideObject(frmP, objIndex);
												
												objIndex = FrmGetObjectIndex(frmP, btnStop);
												FrmShowObject(frmP, objIndex);						
																	
												SndStreamStart(gSndStreamRef);
												//FrmSetFocus(frmP, FrmGetObjectIndex (frmP, btnStop));
												CustomNavOrder(frmP, btnStop);
												FrmSetObjectFocusMode(frmP);

												playing = true;																			
											}											
											MemPtrFree(usrWav);
										}
										else
										{
											VFSFileClose(sndData.fileRef);
											MemPtrFree(usrWav);
										}
									}
								}
							}
						}
					}				
				}											
					break;
				case btnRecord:
				{
					Err error;
					UInt32 freeBytes = 0;
					
					MemCardInfo (0,0,0,0,0,0,0, &freeBytes);

					if(freeBytes > 3000000 || useCard)
					{
					        DateTimeType *dateTimeP = NULL;
						char buffer[5];
														
						dateTimeP = MemPtrNew(sizeof(DateTimeType));
						
						TimSecondsToDateTime (TimGetSeconds(), dateTimeP);
						
						StrCopy(recDesc, months[dateTimeP->month - 1]);
						StrCat(recDesc," ");
						StrIToA(buffer, dateTimeP->day);
						StrCat(recDesc, buffer);
						StrCat(recDesc,", ");
						StrIToA(buffer, dateTimeP->year);
						StrCat(recDesc, buffer);
						StrCat(recDesc," ");
						
						StrIToA(buffer,dateTimeP->hour);
						StrCat(recDesc, buffer);
						StrCat(recDesc, ":");
						StrIToA(buffer,dateTimeP->minute);
						StrCat(recDesc, buffer);
						StrCat(recDesc, ".");
						StrIToA(buffer,dateTimeP->second);
						StrCat(recDesc, buffer);	
						
						MemPtrFree(dateTimeP);											
						sndData.stop = false;
						sndData.dbRef = soundDb;	
						sndData.frameNum = 0;
						sndData.frameCount = 0;
						sndData.recordId = -1;
							  
						if(useCard)
						{
							RecordingType usrRec;	

							int i;
							char fileName[100];
							
							//chiru - make recDesc global variable
							//char recDesc[40];
							/*
							  DateTimeType *dateTimeP = NULL;
														
							  dateTimeP = MemPtrNew(sizeof(DateTimeType));
							
							  TimSecondsToDateTime (TimGetSeconds(), dateTimeP);
							  
							  StrCopy(recDesc, months[dateTimeP->month - 1]);
							  StrCat(recDesc," ");
							  StrIToA(buffer, dateTimeP->day);
							  StrCat(recDesc, buffer);
							  StrCat(recDesc,", ");
							  StrIToA(buffer, dateTimeP->year);
							  StrCat(recDesc, buffer);
							  StrCat(recDesc," ");
							  
							  StrIToA(buffer,dateTimeP->hour);
							  StrCat(recDesc, buffer);
							  StrCat(recDesc, ":");
							  StrIToA(buffer,dateTimeP->minute);
							  StrCat(recDesc, buffer);
							  StrCat(recDesc, ".");
							  StrIToA(buffer,dateTimeP->second);
							  StrCat(recDesc, buffer);	
							  
							  MemPtrFree(dateTimeP);											
							  */
						
							StrCopy(fileName,SDAudioDir);
							StrCat(fileName,"/");
							StrCat(fileName, recDesc);					
							for(i=0;i<StrLen(fileName);i++)
							{
								if(StrNCompare(&fileName[i],":",1) == 0) fileName[i]= '_';
								if(StrNCompare(&fileName[i],".",1) == 0) fileName[i]= '_';
								if(StrNCompare(&fileName[i],",",1) == 0) fileName[i]= '_';
								if(StrNCompare(&fileName[i]," ",1) == 0) fileName[i]= '_';
							}
							StrCat(fileName, ".wav");
							
							usrRec.version = SoundVersion;
							StrCopy(&usrRec.recDesc[0], recDesc);
							usrRec.recordId=0;
							usrRec.frameCount=0;
							usrRec.sampleRate=soundRate;
							usrRec.dataLen=0;						
											
							sndData.playSource = 1;
							sndData.fileRef = CreateWavFile(&fileName[0], &usrRec);
						}
						else
						{
							sndData.playSource = 0;
						}
						sndData.action = record;
								
						error = SndStreamCreate(&gSndStreamRef, sndInput, soundRate, sndInt16Little, sndMono, CaptureSound, &sndData, (UInt32) (2000), false);
						if(!error)
						{
							SndStreamSetVolume (gSndStreamRef, recVol * 500);
							
							//chiru - Hide the list of recordings
							objIndex = FrmGetObjectIndex(frmP, lstRecordings);
							FrmHideObject(frmP, objIndex);
							
							//chiru - Add the name of the file
							addFldString(frmP, fldFileName, recDesc);
							objIndex = FrmGetObjectIndex(frmP, fldFileName);
							FrmShowObject(frmP, objIndex);						
							//chiru - Add the destination of the file
							if(useCard){
							  addFldString(frmP, fldFileDest, "Saving to External SD Card");
							}
							else{
							  addFldString(frmP, fldFileDest, "Saving to Internal Memory");
							}
							objIndex = FrmGetObjectIndex(frmP, fldFileDest);
							FrmShowObject(frmP, objIndex);						

							//chiru - Add the time descriptions
							addFldString(frmP, fldTimeDesc, "Elapsed Time");
							objIndex = FrmGetObjectIndex(frmP, fldTimeDesc);
							FrmShowObject(frmP, objIndex);
							
							addFldString(frmP, fldTimeHr, "00");
							objIndex = FrmGetObjectIndex(frmP, fldTimeHr);
							FrmShowObject(frmP, objIndex);
							
							addFldString(frmP, fldTimeMin, ": 00");
							objIndex = FrmGetObjectIndex(frmP, fldTimeMin);
							FrmShowObject(frmP, objIndex);					
							
							addFldString(frmP, fldTimeSec, ": 00");
							objIndex = FrmGetObjectIndex(frmP, fldTimeSec);
							FrmShowObject(frmP, objIndex);					

							objIndex = FrmGetObjectIndex(frmP, btnPlay);
							FrmHideObject(frmP, objIndex);
					
							objIndex = FrmGetObjectIndex(frmP, btnRecord);
							FrmHideObject(frmP, objIndex);
							
							objIndex = FrmGetObjectIndex(frmP, btnStop);
							FrmShowObject(frmP, objIndex);						
												
							SndStreamStart(gSndStreamRef);
							//FrmSetFocus(frmP, FrmGetObjectIndex (frmP, btnStop));
							CustomNavOrder(frmP, btnStop);
							FrmSetObjectFocusMode(frmP);

							recording = true;	
							timeOn = true;
						}	
					}else
						FrmAlert(MemAlert);
				}								
					break;
				case btnStop:
				        timeOn = false;
					startTime = 0;
				        hr = 0;
					min = 0;
					sec = 0;


					if(recording)					
						sndData.stop = true;
					else
						sndData.action = none;
					break;
			}

			break;
		}
	}
    
	return handled;
}


static Boolean AppHandleEvent(EventType * eventP)
{
	UInt16 formId;
	FormType * frmP;

	if (eventP->eType == frmLoadEvent)
	{
		/* Load the form resource. */
		formId = eventP->data.frmLoad.formID;
		frmP = FrmInitForm(formId);
		FrmSetActiveForm(frmP);

		/* 
		 * Set the event handler for the form.  The handler of the
		 * currently active form is called by FrmHandleEvent each
		 * time is receives an event. 
		 */
		switch (formId)
		{
			case MainForm:
				mainForm = frmP;
				FrmSetEventHandler(frmP, MainFormHandleEvent);
				break;
			case PrefForm:
				FrmSetEventHandler(frmP, PrefFormHandleEvent);
				break;
				

		}
		return true;
	}

	return false;
}

/*
 * FUNCTION: AppEventLoop
 *
 * DESCRIPTION: This routine is the event loop for the application.
 */

static void AppEventLoop(void)
{
	UInt16 error;
	EventType event;

	do 
	{
		/* change timeout if you need periodic nilEvents */
		EvtGetEvent(&event, 1);
		
		if (! SysHandleEvent(&event))
		{
			if (! MenuHandleEvent(0, &event, &error))
			{
				if (! AppHandleEvent(&event))
				{
					FrmDispatchEvent(&event);
				}
			}
		}
	} while (event.eType != appStopEvent);
}

static Err AppStart(void)
{
	LoadPreferences();
	DataInit();
	return errNone;
}


static void AppStop(void)
{   
	if(recording && sndData.action == record)					
	{
		sndData.stop = true;
		
		SndStreamStop(gSndStreamRef);		
		SndStreamDelete(gSndStreamRef);	
		   
		SaveBuffer();
	}
	
	if(playing && sndData.action == play)
	{
		sndData.stop = true;
		SndStreamStop(gSndStreamRef);		
		SndStreamDelete(gSndStreamRef);	
	}
	
	if(audioList != NULL) 
	{
		FreeAudio(audioList);
		audioList = NULL;
	}

	timeOn = false;
	startTime = 0;
	hr = 0;
	min = 0;
	sec = 0;
	
	
	if(soundDb) DmCloseDatabase( soundDb );        
	if(recordingDb) DmCloseDatabase( recordingDb );
	FrmCloseAllForms();
}


static Err RomVersionCompatible(UInt32 requiredVersion, UInt16 launchFlags)
{
	UInt32 romVersion;

	/* See if we're on in minimum required version of the ROM or later. */
	FtrGet(sysFtrCreator, sysFtrNumROMVersion, &romVersion);
	if (romVersion < requiredVersion)
	{
		if ((launchFlags & 
			(sysAppLaunchFlagNewGlobals | sysAppLaunchFlagUIApp)) ==
			(sysAppLaunchFlagNewGlobals | sysAppLaunchFlagUIApp))
		{
			FrmAlert (RomIncompatibleAlert);

			/* Palm OS versions before 2.0 will continuously relaunch this
			 * app unless we switch to another safe one. */
			if (romVersion < kPalmOS20Version)
			{
				AppLaunchWithCommand(
					sysFileCDefaultApp, 
					sysAppLaunchCmdNormalLaunch, NULL);
			}
		}

		return sysErrRomIncompatible;
	}

	return errNone;
}


UInt32 PilotMain(UInt16 cmd, MemPtr cmdPBP, UInt16 launchFlags)
{
	Err error;

	error = RomVersionCompatible (ourMinVersion, launchFlags);
	if (error) return (error);

	switch (cmd)
	{
		case sysAppLaunchCmdNormalLaunch:
			error = AppStart();
			if (error) 
				return error;

			/* 
			 * start application by opening the main form
			 * and then entering the main event loop 
			 */
			FrmGotoForm(MainForm);
			AppEventLoop();

			AppStop();
			break;
		case sysAppLaunchCmdNotify:
			ProcessCmdNotify(cmdPBP); //for one button
			break;			
	}

	return errNone;
}

static Err OpenDatabase(char * DBName, DmOpenRef * DataRef)
{
	Err error = errNone;
	LocalID dbId;
	
	dbId = DmFindDatabase (cardId, DBName);
	
	if(dbId)
	{
		*DataRef = DmOpenDatabase(cardId, dbId, dmModeReadWrite);
		if(!(*DataRef))
		{
			error = DmCreateDatabase(cardId, DBName, libCreatorID, libDBType, false);
			if(!error)
			{
				*DataRef = DmOpenDatabase(cardId, dbId, dmModeReadWrite);
			}
		}
	}else
	{
		error = DmCreateDatabase(cardId, DBName, libCreatorID, libDBType, false);
		if(!error)
		{
			dbId = DmFindDatabase (cardId, DBName);
			if(dbId)
			{
				*DataRef = DmOpenDatabase(cardId, dbId, dmModeReadWrite);
			}else
				return dmErrCantFind;
	
		}
	}
	
	return error;
}

static void DataInit(void)
{
	Err error;
	UInt32	vfsMgrVersion;
	UInt32 	volIterator = vfsIteratorStart;
	
	error = OpenDatabase(sndDBName, &soundDb);
	error = OpenDatabase(recDBName, &recordingDb);

	//register VFS support
	error = FtrGet(sysFileCVFSMgr, vfsFtrIDVersion, &vfsMgrVersion);
	if (!error) 
	{
		error = VFSVolumeEnumerate(&gVolRefNum, &volIterator);		
		if (!error)
		{
			VFSDirCreate(gVolRefNum, "/PALM/Programs");
			VFSDirCreate(gVolRefNum, SDAudioDir);
		}
		else
			gVolRefNum = sysInvalidRefNum;
	}	
}

static void UpdateList(void)
{
	ListType *list = GetObjectPtr(lstRecordings);/*
	UInt16 recCount = DmNumRecords(recordingDb);
	LstSetListChoices(list, NULL, recCount);
	LstDrawList (list);
	LstSetSelection(list, 0);*/
	RefreshList();
	LstDrawList(list);	

}

static void SaveBuffer(void)
{
	if(recording)
	{
			if(sndData.playSource == 0)
			{
			//save recording data
			//char buffer[5];
			UInt16 index = dmMaxRecordIndex;
			MemHandle h;

			//chiru - make recDesc global variable
			/*
			  char recDesc[40];
			  DateTimeType *dateTimeP = NULL;			
			  			
			  dateTimeP = MemPtrNew(sizeof(DateTimeType));
			
			  TimSecondsToDateTime (TimGetSeconds(), dateTimeP);
			
			  StrCopy(recDesc, months[dateTimeP->month - 1]);
			  StrCat(recDesc," ");
			  StrIToA(buffer, dateTimeP->day);
			  StrCat(recDesc, buffer);
			  StrCat(recDesc,", ");
			  StrIToA(buffer, dateTimeP->year);
			  StrCat(recDesc, buffer);
			  StrCat(recDesc," ");
			  
			  StrIToA(buffer,dateTimeP->hour);
			  StrCat(recDesc, buffer);
			  StrCat(recDesc, ":");
			  StrIToA(buffer,dateTimeP->minute);
			  StrCat(recDesc, buffer);
			  StrCat(recDesc, ".");
			  StrIToA(buffer,dateTimeP->second);
			  StrCat(recDesc, buffer);	
			  
			  MemPtrFree(dateTimeP);					
			  */
			
			h = DmNewRecord(recordingDb, &index,  54);
			if(h)
			{
				MemPtr recPtr;
				recPtr = MemHandleLock(h);
				DmWrite(recPtr, 0, &SoundVersion, 1);
				DmWrite(recPtr, 1 , &recDesc[0], 40);
				DmWrite(recPtr, 42, &sndData.recordId, 4);
				DmWrite(recPtr, 46, &sndData.frameCount, 2);
				DmWrite(recPtr, 48, &soundRate, 2);
				DmWrite(recPtr, 50, &sndData.dataLen, 4); 
				MemHandleUnlock(h);
				DmReleaseRecord(recordingDb, index, true);
			}
		}else
		{
			UInt32 fileSize = sndData.dataLen + sizeof(WavType) - 8;
			UInt32 dataSize = sndData.dataLen;
			VFSFileSeek(sndData.fileRef,vfsOriginBeginning, 4);
			ByteSwap32(&fileSize);
			VFSFileWrite(sndData.fileRef, 4, &fileSize, NULL);
			VFSFileSeek(sndData.fileRef, vfsOriginBeginning, sizeof(WavType) - 4);
			ByteSwap32(&dataSize);
			VFSFileWrite(sndData.fileRef, 4, &dataSize, NULL);
			if(sndData.fileRef != NULL)
				VFSFileClose(sndData.fileRef);
		}				
	}
	
	playing = false;
	recording = false;
}


static Boolean PrefFormHandleEvent(EventType * eventP)
{
	Boolean handled = false;
	FormType * frmP = FrmGetActiveForm();
	char buffer[15];	
	
	switch(eventP->eType)
	{
		case popSelectEvent:
			break;
		case frmOpenEvent:
		{
			UInt16 volLev;
			FrmDrawForm(frmP);	
			CtlSetValue(GetObjectPtr(chkUseCard),useCard);
			CtlSetValue(GetObjectPtr(sldSndQal),soundRate/1000);
			CtlSetValue(GetObjectPtr(sldRecVol),recVol);
			CtlSetValue(GetObjectPtr(sldSndVol),playVol);
			CtlSetValue(GetObjectPtr(chkOneButton),oneButton);
			
			volLev = CtlGetValue(GetObjectPtr(sldSndQal));
			StrIToA(buffer, volLev);
			if(volLev == 44)
				StrCat(buffer,".1");
			if(volLev == 22)
				StrCat(buffer,".05");
			if(volLev == 11)
				StrCat(buffer,".025");
			StrCat(buffer, " khz");
			WindowEraseRegion(120,103,40,15);
			WinDrawChars(buffer,StrLen(buffer),120,103);
			
			volLev = CtlGetValue(GetObjectPtr(sldSndVol));
			StrIToA(buffer, volLev);
			WindowEraseRegion(120,50,40,20);
			WinDrawChars(buffer,StrLen(buffer),120,50);
			
			volLev = CtlGetValue(GetObjectPtr(sldRecVol));
			StrIToA(buffer, volLev);
			WindowEraseRegion(120,76,40,20);
			WinDrawChars(buffer,StrLen(buffer),120,76);			
			
			handled = true;
			break;
			}
		case ctlSelectEvent:
			{
				LibPreferenceType prefs;								
				switch(eventP->data.ctlSelect.controlID)
				{
					case sldSndQal:
						{
						UInt16 volLev = CtlGetValue(GetObjectPtr(sldSndQal));
						StrIToA(buffer, volLev);
						if(volLev == 44)
							StrCat(buffer,".1");
						if(volLev == 22)
							StrCat(buffer,".05");
						if(volLev == 11)
							StrCat(buffer,".025");													
						StrCat(buffer, " khz");
						WindowEraseRegion(120,103,40,15);
						WinDrawChars(buffer,StrLen(buffer),120,103);
						break;
						}	
					case sldSndVol:
						{
						UInt16 volLev = CtlGetValue(GetObjectPtr(sldSndVol));
						StrIToA(buffer, volLev);
						WindowEraseRegion(120,50,40,20);
						WinDrawChars(buffer,StrLen(buffer),120,50);
						break;
						}	
					case sldRecVol:
						{
						UInt16 volLev = CtlGetValue(GetObjectPtr(sldRecVol));
						StrIToA(buffer, volLev);
						WindowEraseRegion(120,76,40,20);
						WinDrawChars(buffer,StrLen(buffer),120,76);
						break;
						}																		
					case btnPFSave:						
						prefs.UseCard = CtlGetValue(GetObjectPtr(chkUseCard));						
						prefs.OneButton = CtlGetValue(GetObjectPtr(chkOneButton));	
						prefs.SampleRate = CtlGetValue(GetObjectPtr(sldSndQal));
						prefs.RecVolume = CtlGetValue(GetObjectPtr(sldRecVol));
						prefs.PlayVolume = CtlGetValue(GetObjectPtr(sldSndVol));
						PrefSetAppPreferences(libCreatorID, libPrefID, 0, &prefs, sizeof(prefs), true);						
						if(!(oneButton & prefs.OneButton))
						{
							if(prefs.OneButton)
								RegisterForNotifications();
							else
								UnregisterForNotifications();
						}
						useCard = prefs.UseCard;
						oneButton = prefs.OneButton;
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
						playVol = prefs.PlayVolume;											
					case btnPFCancel:
						FrmReturnToForm(MainForm);
						break;
				}
			}
			break;
	}
	
	return handled;
}


static void LoadPreferences(void)
{
	LibPreferenceType prefs;
	UInt16 prefSize;
	
	prefSize = sizeof(LibPreferenceType);
	if(PrefGetAppPreferences(libCreatorID, libPrefID, &prefs, &prefSize, true) != noPreferenceFound)
	{
		useCard = prefs.UseCard;
		oneButton = prefs.OneButton;
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
		playVol = prefs.PlayVolume;
		recVol = prefs.RecVolume;
		
		if(oneButton)
			RegisterForNotifications();
	}
}

static FileRef CreateWavFile(char * fileName, RecordingType * usrRec)
{
	WavType myWav;
	FileRef fileRef;
	Err err;
	UInt16 frameNum= 0;
	UInt16 recIndex = 0;

	if(gVolRefNum != sysInvalidRefNum)
	{		
	 	VFSFileDelete (gVolRefNum,fileName); 
		err = VFSFileCreate(gVolRefNum, fileName);
		if(!err)
		{					
			err = VFSFileOpen(gVolRefNum, fileName, vfsModeWrite, &fileRef);

			StrCopy(myWav.riff, "RIFF");
			
			myWav.fileLen = usrRec->dataLen + sizeof(WavType) - 8;
			ByteSwap32(&myWav.fileLen);
			
			StrCopy(myWav.wave, "WAVE");
			StrCopy(myWav.fmt, "fmt ");
			
			myWav.fmtLen = 18;
			ByteSwap32(&myWav.fmtLen);
			
			myWav.fmtTag = 1;
			ByteSwap16(&myWav.fmtTag);
			
			myWav.channels = 1;
			ByteSwap16(&myWav.channels);
			
			myWav.sampleRate = usrRec->sampleRate;
			ByteSwap32(&myWav.sampleRate);
			
			
			myWav.bytesSec = usrRec->sampleRate * 2;
			ByteSwap32(&myWav.bytesSec);
			
			myWav.blockAlign = 2;
			ByteSwap16(&myWav.blockAlign);
			
			myWav.bits = 16;					
			ByteSwap16(&myWav.bits);
			
			myWav.extraBytes = 0;
			
			StrCopy(myWav.fact, "fact");
			
			myWav.factSize = 4;
			ByteSwap32(&myWav.factSize);
			
			myWav.factData = usrRec->sampleRate;
			ByteSwap32(&myWav.factData);			
			
			StrCopy(myWav.data, "data");
			
			myWav.dataLen = usrRec->dataLen;
			ByteSwap32(&myWav.dataLen);
			
			VFSFileWrite (fileRef, sizeof(WavType), &myWav, NULL);
			
			if(usrRec->recordId != 0)
			{
				WinHandle oldWin = 	InitProgressBar();
				err = DmFindRecordByID( soundDb, usrRec->recordId, &recIndex);
				if(!err && (frameNum < usrRec->frameCount))
				{				
					MemHandle recordH;
					void * theRecPtr;
					UInt32 theSize=0;
					Boolean stop = false;
					while(!stop && (frameNum < usrRec->frameCount))
					{
						UpdateProgressBar(((double) frameNum / usrRec->frameCount) * 100);
						recordH = DmQueryRecord(soundDb, recIndex + frameNum);
						if(recordH)
						{					
						    theRecPtr = MemHandleLock( recordH );
						    theSize = MemPtrSize(theRecPtr) - 2;
						    VFSFileWrite(fileRef, theSize, (void *) ((UInt8 *) theRecPtr + 2), NULL);
						    MemHandleUnlock( recordH );
						    frameNum++;			
					    }else
					    	stop = true;
					}
				}
				KillProgressBar(oldWin);
			}
			
			return fileRef;
		}
	}
	return NULL;	
}

static void ByteSwap32(UInt32 *bytes)
{
	UInt8 t;
	MemMove(&t, bytes, 1);
	MemMove((void *) ((UInt8 *) bytes), (void *) ((UInt8 *) bytes + 3), 1);
	MemMove((void *) ((UInt8 *) bytes + 3), &t, 1);
	
	MemMove(&t, ((UInt8 *) bytes + 1), 1);
	MemMove((void *) ((UInt8 *) bytes + 1), (void *) ((UInt8 *) bytes + 2), 1);
	MemMove((void *) ((UInt8 *) bytes + 2), &t, 1);	
}

static void ByteSwap16(UInt16 *bytes)
{
	UInt8 t;
	MemMove(&t, bytes, 1);
	MemMove((void *) ((UInt8 *) bytes), (void *) ((UInt8 *) bytes + 1), 1);
	MemMove((void *) ((UInt8 *) bytes + 1), &t, 1);
}

static void WindowEraseRegion (int l, int t, int w, int h)
{
  RectangleType	r;
  RGBColorType		rgbColor;
  IndexedColorType	color;

  RctSetRectangle (&r, l, t, w, h);

  rgbColor.index = 0;	// index of color or best match to cur CLUT or unused.
  rgbColor.r = 255;		// amount of red, 0->255
  rgbColor.g = 255;		// amount of green, 0->255
  rgbColor.b = 255;		// amount of blue, 0->255
  color = WinRGBToIndex (&rgbColor);

  color =  WinSetForeColor (color);
  WinDrawRectangle (&r, 0);        
}

static void RefreshList(void)
{
	ListType *list = GetObjectPtr(lstRecordings);
	struct AudioRecList* cAudio;
	FormType * frmP;
	int i;

	frmP = FrmGetActiveForm();
		
	if(audioList != NULL) 
	{
		FreeAudio(audioList);
		audioList = NULL;
	}
	
	audioListCount = 0;

	for(i = 0;i< DmNumRecords(recordingDb);i++)
	{
		MemHandle h;
		UInt32 recDbId = 0;
		h = DmQueryRecord(recordingDb, i);		
		if(h)
		{
			UInt16 recNum = 0;
			RecordingType * usrRec;
			usrRec = (RecordingType *) MemHandleLock(h);
			if(audioList == NULL)
			{
				audioList = NewAudio();
				cAudio = audioList;
			}
			else
			{
				cAudio = audioList;
				while(cAudio->next != NULL)
					cAudio = cAudio->next;
				cAudio->next = NewAudio();
				cAudio = cAudio->next;					
			}			
			DmRecordInfo (recordingDb, i, NULL, &recDbId, NULL);
			cAudio->Location= 0;
			cAudio->RecordingDbId = recDbId;
			cAudio->Version = (*usrRec).version;
			cAudio->DescLen = StrLen((*usrRec).recDesc);
			cAudio->Description = MemPtrNew(40);
			StrCopy(cAudio->Description, (*usrRec).recDesc);
			DmRecordInfo (soundDb, recNum, NULL, &(cAudio->RecordId), NULL);
			MemHandleUnlock(h);
		}
		audioListCount++;
	}	
	
	if(gVolRefNum != sysInvalidRefNum)
	{
		FileInfoType info;
		FileRef dirRef;
		UInt32 dirIterator;							
		Char *fileName = MemPtrNew(256); 
		UInt16 cFileNum = 1;
		Err err;

		err = VFSFileOpen(gVolRefNum, SDAudioDir, vfsModeRead, &dirRef);
		if(!err)
		{
		    info.nameP = fileName;    // point to local buffer 
		    info.nameBufLen = 255;
			dirIterator = vfsIteratorStart;
			while(dirIterator != vfsIteratorStop)
			{
				err = VFSDirEntryEnumerate(dirRef, &dirIterator, &info);
				if(!err)
				{					
					if(audioList == NULL)
					{
						audioList = NewAudio();
						cAudio = audioList;
					}
					else
					{
						cAudio = audioList;
						while(cAudio->next != NULL)
							cAudio = cAudio->next;
						cAudio->next = NewAudio();
						cAudio = cAudio->next;					
					}	
											
					cAudio->Location = 1;
					cAudio->RecordId = 0;
					cAudio->RecordingDbId = 0;
					cAudio->DescLen = StrLen(fileName);
					cAudio->Description = MemPtrNew(cAudio->DescLen + 1);
					StrCopy(cAudio->Description, fileName);
					audioListCount++;
				}
			}
			VFSFileClose(dirRef);   			 
		}
		MemPtrFree(fileName);																										
	}		
	
	LstSetListChoices(list, NULL, audioListCount);
	if(lastHighlight >= audioListCount) lastHighlight = 0;
	LstSetSelection(list, lastHighlight);	
}

static void FreeAudio(audio * al)
{
	if(al != NULL)
	{
		if(al->next !=NULL)
			FreeAudio(al->next);

		if(al->Description != NULL)
			MemPtrFree(al->Description);
		MemPtrFree(al);
		MemSet(al, sizeof(audio), 0);
		al = NULL;
		audioListCount--;
	}
	return;
}

static audio * GetAudio(int index)
{
	audio * cAudio = audioList;
	UInt8 i;
	for(i = 0; i < index; i++)
		if(cAudio != NULL) cAudio = cAudio->next;
	return cAudio;
}

static audio * NewAudio(void)
{
	audio * newAud;
	newAud = MemPtrNew(sizeof(audio));
	MemSet(newAud, sizeof(audio), 0);
	newAud->next = NULL;
	return newAud;
}


static WinHandle InitProgressBar(void)
{
	WinHandle orgWindow;
	RectangleType newBounds, savedBounds;
	Err err;
	
	newBounds.topLeft.x = 20;
	newBounds.topLeft.y = 60;
	newBounds.extent.x = 120;
	newBounds.extent.y = 20;
	
	progWin = WinCreateWindow(&newBounds, boldRoundFrame, false, false, &err);
	if(!err)
	{
		WinGetWindowFrameRect(progWin, &savedBounds);
		savedWin = WinSaveBits(&savedBounds, &err);
		orgWindow = WinSetDrawWindow(progWin);
		WinEraseWindow();
		WinDrawWindowFrame();					
		return orgWindow;
	}
	else return NULL;		
}

static void UpdateProgressBar(int PercentComplete)
{
	RectangleType rect;
	RGBColorType color, oldColor;
	
	PercentComplete = PercentComplete > 0 ? PercentComplete : 1;
	
	color.index = 50;
	color.r = 12;
	color.b = 200;
	color.g = 110;
	
	rect.topLeft.x = 5;
	rect.topLeft.y = 5;
	rect.extent.x = PercentComplete + (PercentComplete * .1);
	rect.extent.y = 10;
	
	WinSetForeColorRGB(&color, &oldColor);
	WinDrawRectangle(&rect, 1);
	WinSetForeColorRGB(&oldColor, NULL);		
}

static void KillProgressBar(WinHandle orgWin)
{
	if(orgWin != NULL)
	{
		WinSetDrawWindow(orgWin);
		WinDeleteWindow(progWin, true);	
		WinRestoreBits(savedWin, 20,60);
		progWin = NULL;
	}
}

static Boolean DeleteSelection(void)
{
	Err error;

	UInt16 selItem = 0;
	ListType *list = GetObjectPtr(lstRecordings);
	UInt16 recIndex = 0;

	if(LstGetSelection(list) >= 0 && audioList != NULL)
	{
		if(FrmAlert(DelAlert) == 0)
		{
			audio * cAudio;
			cAudio = GetAudio(LstGetSelection(list));
			
			if(cAudio->Location == 0)
			{
				MemHandle h;
				h = DmQueryRecord(recordingDb, LstGetSelection(list));
				if(h)
				{
					RecordingType * usrRec;
					usrRec = (RecordingType *) MemHandleLock(h);
					error = DmFindRecordByID( soundDb, usrRec->recordId, &recIndex);
					if(!error)
					{		
						int i;
						for(i=usrRec->frameCount-1;i>=0;i--)
						{																	
							DmRemoveRecord(soundDb, recIndex + i);						
						}
					}
					MemHandleUnlock(h);
					DmReleaseRecord(recordingDb, LstGetSelection(list), false);
					DmRemoveRecord(recordingDb, LstGetSelection(list));
				}
			}
			
			if(cAudio->Location > 0)
			{
				if(gVolRefNum != sysInvalidRefNum)
					{
						FileRef dirRef;
						char filePath[270];
						Char *fileName = MemPtrNew(256); 
						UInt16 cFileNum = 1;
						Err err;

						err = VFSFileOpen(gVolRefNum, SDAudioDir, vfsModeRead, &dirRef);
						if(!err)
						{
							StrCopy(filePath, SDAudioDir);
							StrCat(filePath, "/");
							StrCat(filePath, cAudio->Description);
							VFSFileDelete(gVolRefNum, filePath);
							VFSFileClose(dirRef);   			 
						}
						MemPtrFree(fileName);																										
					}					
			}	
			return true;
		}
		else
			return false;
	}else
		return false;
}

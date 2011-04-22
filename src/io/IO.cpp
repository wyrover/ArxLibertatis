/*
===========================================================================
ARX FATALIS GPL Source Code
Copyright (C) 1999-2010 Arkane Studios SA, a ZeniMax Media company.

This file is part of the Arx Fatalis GPL Source Code ('Arx Fatalis Source Code'). 

Arx Fatalis Source Code is free software: you can redistribute it and/or modify it under the terms of the GNU General Public 
License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

Arx Fatalis Source Code is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied 
warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with Arx Fatalis Source Code.  If not, see 
<http://www.gnu.org/licenses/>.

In addition, the Arx Fatalis Source Code is also subject to certain additional terms. You should have received a copy of these 
additional terms immediately following the terms and conditions of the GNU General Public License which accompanied the Arx 
Fatalis Source Code. If not, please request a copy in writing from Arkane Studios at the address below.

If you have questions concerning this license or the applicable additional terms, you may contact in writing Arkane Studios, c/o 
ZeniMax Media Inc., Suite 120, Rockville, Maryland 20850 USA.
===========================================================================
*/
//////////////////////////////////////////////////////////////////////////////////////
//   @@        @@@        @@@                @@                           @@@@@     //
//   @@@       @@@@@@     @@@     @@        @@@@                         @@@  @@@   //
//   @@@       @@@@@@@    @@@    @@@@       @@@@      @@                @@@@        //
//   @@@       @@  @@@@   @@@  @@@@@       @@@@@@     @@@               @@@         //
//  @@@@@      @@  @@@@   @@@ @@@@@        @@@@@@@    @@@            @  @@@         //
//  @@@@@      @@  @@@@  @@@@@@@@         @@@@ @@@    @@@@@         @@ @@@@@@@      //
//  @@ @@@     @@  @@@@  @@@@@@@          @@@  @@@    @@@@@@        @@ @@@@         //
// @@@ @@@    @@@ @@@@   @@@@@            @@@@@@@@@   @@@@@@@      @@@ @@@@         //
// @@@ @@@@   @@@@@@@    @@@@@@           @@@  @@@@   @@@ @@@      @@@ @@@@         //
// @@@@@@@@   @@@@@      @@@@@@@@@@      @@@    @@@   @@@  @@@    @@@  @@@@@        //
// @@@  @@@@  @@@@       @@@  @@@@@@@    @@@    @@@   @@@@  @@@  @@@@  @@@@@        //
//@@@   @@@@  @@@@@      @@@      @@@@@@ @@     @@@   @@@@   @@@@@@@    @@@@@ @@@@@ //
//@@@   @@@@@ @@@@@     @@@@        @@@  @@      @@   @@@@   @@@@@@@    @@@@@@@@@   //
//@@@    @@@@ @@@@@@@   @@@@             @@      @@   @@@@    @@@@@      @@@@@      //
//@@@    @@@@ @@@@@@@   @@@@             @@      @@   @@@@    @@@@@       @@        //
//@@@    @@@  @@@ @@@@@                          @@            @@@                  //
//            @@@ @@@                           @@             @@        STUDIOS    //
//////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////
// HERMESMain
//////////////////////////////////////////////////////////////////////////////////////
//
// Description:
//		HUM...hum...
//
// Updates: (date) (person) (update)
//
// Code: Cyril Meynier
//
// Copyright (c) 1999 ARKANE Studios SA. All rights reserved
//////////////////////////////////////////////////////////////////////////////////////

#include "io/IO.h"

#include <cstring>
#include <cstdio>
#include <ctime>
#include <algorithm>
#include <iostream>
#include <algorithm>

#include <shlobj.h>
#include <windows.h>

#include "io/Filesystem.h"
#include "io/Logger.h"

#include "platform/Platform.h"

using std::string;
using std::copy;
using std::transform;

// TODO(lubosz): temporary include replacement
#if !ARX_COMPILER_MSVC
    #define _MAX_FNAME 512
#endif

HWND MAIN_PROGRAM_HANDLE = NULL;

//******************************************************************************
// MEMORY MANAGEMENT
//******************************************************************************
#ifdef _DEBUG
long HERMES_KEEP_MEMORY_TRACE = 0;
#else
long HERMES_KEEP_MEMORY_TRACE = 0;
#endif
#define SIZE_STRING_MEMO_TRACE 24
struct MEMO_TRACE
{
	void 	*		ptr;
	char			string1[SIZE_STRING_MEMO_TRACE];
	unsigned long	size;
};

MEMO_TRACE * MemoTraces = NULL;
long nb_MemoTraces = 0;
 
void HERMES_UnRegister_Memory(void * adr)
{
	long pos = -1;

	for (long i = 0; i < nb_MemoTraces; i++)
	{
		if (MemoTraces[i].ptr == adr)
		{
			pos = i;
			break;
		}
	}

	if (pos == -1) return;

	if (nb_MemoTraces == 1)
	{
		free(MemoTraces);
		MemoTraces = NULL;
		nb_MemoTraces = 0;
		return;
	}

	MEMO_TRACE * tempo = (MEMO_TRACE *)malloc(sizeof(MEMO_TRACE) * (nb_MemoTraces - 1));

	if (tempo == NULL)
	{
		HERMES_KEEP_MEMORY_TRACE = 0;
		free(MemoTraces);
		return;
	}

	for (int i = 0; i < nb_MemoTraces - 1; i++)
	{
		if (i >= pos)
		{
			memcpy(&MemoTraces[i], &MemoTraces[i+1], sizeof(MEMO_TRACE));
		}
	}

	memcpy(tempo, MemoTraces, sizeof(MEMO_TRACE)*(nb_MemoTraces - 1));
	free(MemoTraces);
	MemoTraces = (MEMO_TRACE *)tempo;
	nb_MemoTraces--;
}

unsigned long MakeMemoryText(char * text)
{
	if (HERMES_KEEP_MEMORY_TRACE == 0)
	{
		strcpy(text, "Memory Tracing OFF.");
		return 0;
	}

	bool * ignore;
	unsigned long TotMemory = 0;
	unsigned long TOTTotMemory = 0;
	char header[128];
	char theader[128];

	if (nb_MemoTraces == 0)
	{
		text[0] = 0;
		return 0;
	}

	ignore = (bool *)malloc(sizeof(bool) * nb_MemoTraces);

	for (long i = 0; i < nb_MemoTraces; i++) ignore[i] = false;

	strcpy(text, "");

	for (int i = 0; i < nb_MemoTraces; i++)
	{
		if (!ignore[i])
		{
			TotMemory = MemoTraces[i].size;

			if (MemoTraces[i].string1[0] != 0)
				strcpy(header, MemoTraces[i].string1);
			else strcpy(header, "<Unknown>");

			for (long j = i + 1; j < nb_MemoTraces; j++)
			{
				if (!ignore[j])
				{
					if (MemoTraces[j].string1[0] != 0)
						strcpy(theader, MemoTraces[j].string1);
					else strcpy(theader, "<Unknown>");

					if (!strcmp(header, theader))
					{
						ignore[j] = true;
						TotMemory += MemoTraces[j].size;
					}
				}
			}

			sprintf(theader, "%12lu %s\r\n", TotMemory, header);

			if (strlen(text) + strlen(theader) + 4 < 64000)
			{
				strcat(text, theader);
			}

			TOTTotMemory += TotMemory;
		}
	}

	free(ignore);
	return TOTTotMemory;
}

void MemFree(void * adr)
{
	if (HERMES_KEEP_MEMORY_TRACE)
		HERMES_UnRegister_Memory(adr);

	free(adr);
}
 
bool HERMES_CreateFileCheck(const char * name, char * scheck, size_t size, const float id)
{
	LogWarning << "partially unimplemented HERMES_CreateFileCheck";
	
	
	printf("HERMES_CreateFileCheck(%s, ...)\n", name);
	WIN32_FILE_ATTRIBUTE_DATA attrib;
	FileHandle file;
	long length(size >> 2), i = 7 * 4;

	// TODO port
	if (!GetFileAttributesEx(name, GetFileExInfoStandard, &attrib)) return true;

	if (!(file = FileOpenRead(name))) return true;

	FileSeek(file, 0, SEEK_END);

	memset(scheck, 0, size);
	((float *)scheck)[0] = id;
	((long *)scheck)[1] = size;
	memcpy(&((long *)scheck)[2], &attrib.ftCreationTime, sizeof(FILETIME));
	memcpy(&((long *)scheck)[4], &attrib.ftLastWriteTime, sizeof(FILETIME));
	((long *)scheck)[6] = FileTell(file);
	memcpy(&scheck[i], name, strlen(name) + 1);
	i += strlen(name) + 1;
	memset(&scheck[i], 0, i % 4);
	i += i % 4;
	i >>= 2;

	FileSeek(file, 0, SEEK_SET);

	while (i < length)
	{
		long crc = 0;
		
		for (long j = 0; j < 256; j++)
		{
			char read;

			if (!FileRead(file, &read, 1)) break;

			crc += read;
		}

		((long *)scheck)[i++] = crc;

		// TODO is this actually needed?
		//if (feof(file))
		//{
		//	memset(&((long *)scheck)[i], 0, (length - i) << 2);
		//	break;
		//}
	}

	FileClose(file);

	return false;
}

//******************************************************************************
// OPEN/SAVE FILES DIALOGS
//******************************************************************************
char	LastFolder[MAX_PATH];		// Last Folder used
static OPENFILENAME ofn;

bool HERMESFolderBrowse(const char * str)
{
	BROWSEINFO		bi;
	LPITEMIDLIST	liil;

	bi.hwndOwner	= NULL;//MainFrameWindow;
	bi.pidlRoot		= NULL;
	bi.pszDisplayName = LastFolder;
	bi.lpszTitle	= str;
	bi.ulFlags		= 0;
	bi.lpfn			= NULL;
	bi.lParam		= 0;
	bi.iImage		= 0;


	liil = SHBrowseForFolder(&bi);

	if (liil)
	{
		if (SHGetPathFromIDList(liil, LastFolder))	return true;
		else return false;
	}
	else return false;
}


bool HERMESFolderSelector(char * file_name, const char * title)
{
	if (HERMESFolderBrowse(title))
	{
		sprintf(file_name, "%s\\", LastFolder);
		return true;
	}
	else
	{
		strcpy(file_name, " ");
		return false;
	}
}
bool HERMES_WFSelectorCommon(const char * pstrFileName, const char * pstrTitleName, const char * filter, long flag, long flag_operation, long max_car, HWND hWnd)
{
	BOOL	value;
	char	cwd[MAX_PATH];

	ofn.lStructSize		= sizeof(OPENFILENAME) ;
	ofn.hInstance			= NULL ;
	ofn.lpstrCustomFilter	= NULL ;
	ofn.nMaxCustFilter		= 0 ;
	ofn.nFilterIndex		= 0 ;
	ofn.lpstrFileTitle		= NULL ;
	ofn.nMaxFileTitle		= _MAX_FNAME + MAX_PATH ;
	ofn.nFileOffset		= 0 ;
	ofn.nFileExtension		= 0 ;
	ofn.lpstrDefExt		= "txt" ;
	ofn.lCustData			= 0L ;
	ofn.lpfnHook			= NULL ;
	ofn.lpTemplateName		= NULL ;

	ofn.lpstrFilter			= filter ;
	ofn.hwndOwner			= hWnd;
	ofn.lpstrFile			= strdup(pstrFileName);
	ofn.lpstrTitle			= pstrTitleName ;
	ofn.Flags				= flag;

	GetCurrentDirectory(MAX_PATH, cwd);
	ofn.lpstrInitialDir = cwd;
	ofn.nMaxFile = max_car;

	if (flag_operation)
	{
		value = GetOpenFileName(&ofn);
	}
	else
	{
		value = GetSaveFileName(&ofn);
	}
	
	free(ofn.lpstrFile);

	return value == TRUE;
}

int HERMESFileSelectorOpen(const char * pstrFileName, const char * pstrTitleName, const char * filter, HWND hWnd)
{
	return HERMES_WFSelectorCommon(pstrFileName, pstrTitleName, filter, OFN_HIDEREADONLY | OFN_CREATEPROMPT, 1, MAX_PATH, hWnd);
}
 
int HERMESFileSelectorSave(const char * pstrFileName, const char * pstrTitleName, const char * filter, HWND hWnd)
{
	return HERMES_WFSelectorCommon(pstrFileName, pstrTitleName, filter, OFN_OVERWRITEPROMPT, 0, MAX_PATH, hWnd);
}
 
//-------------------------------------------------------------------------------------
// SP funcs
#define hrnd()  (((float)rand() ) * 0.00003051850947599f)

//-------------------------------------------------------------------------------------
// Error Logging Funcs...
char * HERMES_MEMORY_SECURITY = NULL;
void HERMES_Memory_Security_On(long size)
{
	if (size < 128000) size = 128000;

	HERMES_MEMORY_SECURITY = (char *)malloc(size);
}
void HERMES_Memory_Security_Off()
{
	if (HERMES_MEMORY_SECURITY)
		free(HERMES_MEMORY_SECURITY);

	HERMES_MEMORY_SECURITY = NULL;
}

long HERMES_Memory_Emergency_Out( long size, const std::string& info )
{
	/* TODO Is HERMES_MEMORY_SECURITY still useful? */
	if (HERMES_MEMORY_SECURITY) 
		free(HERMES_MEMORY_SECURITY);

	HERMES_MEMORY_SECURITY = NULL;

	if ( size > 0 )
	{
		LogError << "FATAL ERROR: Unable to To Allocate " << size << " bytes..." << info;
		return 1;
	}
	else
	{
		LogError << "FATAL ERROR: Unable to To Allocate Memory..." << info;
		exit(0);
		return 0; // Never reached
	}
}

LARGE_INTEGER	start_chrono;
long NEED_BENCH = 0;
void StartBench()
{
	if (NEED_BENCH)
	{
		QueryPerformanceCounter(&start_chrono);
	}
}
unsigned long EndBench()
{
	if (NEED_BENCH)
	{
		LARGE_INTEGER	end_chrono;
		QueryPerformanceCounter(&end_chrono);
		unsigned long ret = (unsigned long)(end_chrono.QuadPart - start_chrono.QuadPart);
		return ret;
	}

	return 0;
}


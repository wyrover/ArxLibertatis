/*
 * Copyright 2011 Arx Libertatis Team (see the AUTHORS file)
 *
 * This file is part of Arx Libertatis.
 *
 * Arx Libertatis is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Arx Libertatis is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Arx Libertatis.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "crashreporter/errorreport.h"

#ifdef HAVE_WINAPI
// Win32
#include <winsock2.h>
#include <windows.h>
#include <dbghelp.h>
#include <psapi.h>
#else
#include <sys/wait.h>
#endif

// Qt
#include <QApplication>
#include <QMessageBox>
#include <QDesktopWidget>
#include <QDir>
#include <QProcess>
#include <QTime>
#include <QFile>
#include <QFileInfo>
#include <QFileInfoList>
#include <QThread>
#include <QXmlStreamWriter>

#include "io/fs/Filesystem.h"
#include "io/fs/FileStream.h"
#include "platform/Thread.h"

// CrashReporter
#include "crashreporter/utilities_win32.h"

// Zip
#include "crashreporter/minizip/zip.h"

// CSmtp
#include "crashreporter/csmtp/csmtp.h"

ErrorReport::ErrorReport(const std::string& sharedMemoryName)
	: m_RunningTimeSec()
	, m_ProcessIs64Bit()
	, m_SharedMemoryName(sharedMemoryName)
	, m_pCrashInfo()
{
}

bool ErrorReport::Initialize()
{	
	// Create a shared memory object.
	m_SharedMemory = boost::interprocess::shared_memory_object(boost::interprocess::open_only, m_SharedMemoryName.c_str(), boost::interprocess::read_write);

	// Map the whole shared memory in this process
	m_MemoryMappedRegion = boost::interprocess::mapped_region(m_SharedMemory, boost::interprocess::read_write);

	// Our SharedCrashInfo will be stored in this shared memory.
	m_pCrashInfo = (CrashInfo*)m_MemoryMappedRegion.get_address();

	if(m_MemoryMappedRegion.get_size() != sizeof(CrashInfo))
		return false;

	bool bMiscCrashInfo = GetMiscCrashInfo();
	if(!bMiscCrashInfo)
		return false;

	// Add attached files from the report
	for(int i = 0; i < m_pCrashInfo->nbFilesAttached; i++)
		AddFile(m_pCrashInfo->attachedFiles[i]);

	m_ReportFolder = fs::path(m_pCrashInfo->crashReportFolder) / fs::path(m_CrashDateTime.toString("yyyy.MM.dd hh.mm.ss").toAscii());

	if(!fs::create_directories(m_ReportFolder))
		return false;

	return true;
}

bool ErrorReport::GetScreenshot(const fs::path& fileName, int quality, bool bGrayscale)
{
	fs::path fullPath = m_ReportFolder / fileName;

#ifdef HAVE_WINAPI
	WId mainWindow = GetMainWindow(m_pCrashInfo->processId);
	
	if(mainWindow == 0)
		return false;
	
	RECT r;
	GetWindowRect(mainWindow, &r);

	QPixmap pixmap = QPixmap::grabWindow(QApplication::desktop()->winId(), r.left, r.top, r.right - r.left, r.bottom - r.top);
#else
	QPixmap pixmap = QPixmap::grabWindow(QApplication::desktop()->winId());
#endif
	
	if(bGrayscale)
	{
		QImage image = pixmap.toImage();
		QRgb col;
		int gray;
		int width = pixmap.width();
		int height = pixmap.height();
		for (int i = 0; i < width; ++i)
		{
			for (int j = 0; j < height; ++j)
			{
				col = image.pixel(i, j);
				gray = qGray(col);
				image.setPixel(i, j, qRgb(gray, gray, gray));
			}
		}
		pixmap = pixmap.fromImage(image);
	}

	bool bSaved = pixmap.save(fullPath.string().c_str(), 0, quality);
	if(bSaved)
		AddFile(fullPath);

	return bSaved;
}

#ifdef HAVE_WINAPI
// This callbask function is called by MinidumpWriteDump
BOOL CALLBACK MiniDumpCallback(PVOID CallbackParam, PMINIDUMP_CALLBACK_INPUT CallbackInput, PMINIDUMP_CALLBACK_OUTPUT CallbackOutput)
{
	return TRUE;
}
#endif

bool ErrorReport::GetCrashDump(const fs::path& fileName) {
	
#ifdef HAVE_WINAPI
	
	fs::path fullPath = m_ReportFolder / fileName;
	
	MINIDUMP_EXCEPTION_INFORMATION exceptionInfo;
	MINIDUMP_CALLBACK_INFORMATION callbackInfo;

	EXCEPTION_POINTERS exceptionPointers;
	exceptionPointers.ExceptionRecord = &m_pCrashInfo->exceptionRecord;
	exceptionPointers.ContextRecord = &m_pCrashInfo->contextRecord;

	// Write minidump to the file
	exceptionInfo.ThreadId = m_pCrashInfo->threadId;
	exceptionInfo.ExceptionPointers = &exceptionPointers;
	exceptionInfo.ClientPointers = FALSE;
  
	callbackInfo.CallbackRoutine = MiniDumpCallback;
	callbackInfo.CallbackParam = 0;

	HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, m_pCrashInfo->processId);
	if(hProcess == INVALID_HANDLE_VALUE)
		return false;

	// Create the minidump file
	HANDLE hFile = CreateFile(fullPath.string().c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if(hFile == INVALID_HANDLE_VALUE)
		return false;

	BOOL bWriteDump = MiniDumpWriteDump(hProcess, m_pCrashInfo->processId, hFile, m_pCrashInfo->miniDumpType, &exceptionInfo, NULL, &callbackInfo);

	CloseHandle(hFile);

	if(bWriteDump)
		AddFile(fullPath);

	return bWriteDump;
	
#else // !HAVE_WINAPI
	
	ARX_UNUSED(fileName);
	// TODO: Write core dump to 
	// fs::path fullPath = m_ReportFolder / fileName;
	
#if defined(HAVE_FORK) && defined(HAVE_EXECLP) && defined(HAVE_DUP2)
	
	fs::path tracePath = m_ReportFolder / "gdbtrace.txt";
	
	// Fork so we retain control after launching GDB.
	int childPID = fork();
	if(childPID) {
		// Wait for GDB to exit.
		waitpid(childPID, NULL, 0);
	} else {
		
		// Redirect output to a file
		int fd = open(tracePath.string().c_str(), O_WRONLY|O_CREAT, 0666);
		dup2(fd, 1);
		close(fd);
		
		// Prepare pid argument for GDB.
		char pid_buf[30];
		memset(&pid_buf, 0, sizeof(pid_buf));
		sprintf(pid_buf, "%d", m_pCrashInfo->processId);
		
		// Try to execute gdb to get a very detailed stack trace.
		execlp("gdb", "gdb", "--batch", "-n", "-ex", "thread", "-ex", "set confirm off", "-ex", "set print frame-arguments all", "-ex", "set print static-members off", "-ex", "thread apply all bt full", m_pCrashInfo->execFullName, pid_buf, NULL);
		
		// GDB failed to start.
		exit(1);
	}
#endif // defined(HAVE_EXECLP) && defined(HAVE_DUP2)
	
	bool bWroteDump = fs::exists(tracePath) && fs::file_size(tracePath) > 0;
	if(bWroteDump) {
		AddFile(tracePath);
		return true;
	}
	
	tracePath = m_ReportFolder / "trace.txt";
	fs::ofstream ofs(tracePath, ios_base::trunc);
	ofs << safestring(m_pCrashInfo->backtrace);
	ofs.flush();
	ofs.close();
	
	AddFile(tracePath);
	
	return true;
	
#endif // !HAVE_WINAPI
}

bool ErrorReport::GetMachineInfo(const fs::path& fileName)
{
	fs::path fullPath = m_ReportFolder / fileName;

	QString program = "dxdiag.exe";
	QStringList arguments;
	arguments << "/whql:off"
			  << "/64bit"
			  << "/t" << fullPath.string().c_str();

	QStringList env = QProcess::systemEnvironment();

	QTime timeWatch;
	timeWatch.start();

	QProcess *myProcess = new QProcess();
	myProcess->start(program, arguments);
	bool bStarted = myProcess->waitForStarted(10000);
	if(!bStarted)
		return false;

	// Writing the DXDiag file can take quite some time...
	while(!fs::exists(fullPath) && timeWatch.elapsed() < 120000)
	{
		Thread::sleep(1000);
	}

	bool bExists = fs::exists(fullPath);
	if(bExists)
		AddFile(fullPath);

	return bExists;
}

#ifdef HAVE_WINAPI
static BOOL CALLBACK LoadModuleCB(PCSTR ModuleName, DWORD64 ModuleBase, ULONG ModuleSize, PVOID UserContext)
{
    SymLoadModule64((HANDLE)UserContext, 0, ModuleName, ModuleName, ModuleBase, ModuleSize);
    return TRUE;
}

std::string GetCallStack(HANDLE hProcess, HANDLE hThread, PCONTEXT pContext)
{
	DWORD options = SymGetOptions();
	options |= SYMOPT_LOAD_LINES;
    options &= ~SYMOPT_DEFERRED_LOADS;
    options &= ~SYMOPT_UNDNAME;
	SymSetOptions (options);

	SymInitialize(hProcess, NULL, FALSE);

	STACKFRAME64 stackFrame;
	memset(&stackFrame, 0, sizeof(stackFrame));

	DWORD imageType;

#ifdef _M_IX86
	imageType = IMAGE_FILE_MACHINE_I386;
	stackFrame.AddrPC.Offset = pContext->Eip;
	stackFrame.AddrPC.Mode = AddrModeFlat;
	stackFrame.AddrFrame.Offset = pContext->Ebp;
	stackFrame.AddrFrame.Mode = AddrModeFlat;
	stackFrame.AddrStack.Offset = pContext->Esp;
	stackFrame.AddrStack.Mode = AddrModeFlat;
#elif _M_X64
	imageType = IMAGE_FILE_MACHINE_AMD64;
	stackFrame.AddrPC.Offset = pContext->Rip;
	stackFrame.AddrPC.Mode = AddrModeFlat;
	stackFrame.AddrFrame.Offset = pContext->Rsp;
	stackFrame.AddrFrame.Mode = AddrModeFlat;
	stackFrame.AddrStack.Offset = pContext->Rsp;
	stackFrame.AddrStack.Mode = AddrModeFlat;
#else
	#error "Unsupported"
#endif

	const size_t STACKWALK_MAX_NAMELEN = 1024;
	IMAGEHLP_SYMBOL64* pSymbol = (IMAGEHLP_SYMBOL64*)malloc(sizeof(IMAGEHLP_SYMBOL64) + STACKWALK_MAX_NAMELEN);
	memset(pSymbol, 0, sizeof(IMAGEHLP_SYMBOL64) + STACKWALK_MAX_NAMELEN);
	pSymbol->SizeOfStruct = sizeof(IMAGEHLP_SYMBOL64);
	pSymbol->MaxNameLength = STACKWALK_MAX_NAMELEN;
	
	CHAR undFullName[STACKWALK_MAX_NAMELEN];

	IMAGEHLP_LINE64 Line;
    memset( &Line, 0, sizeof(Line) );
	Line.SizeOfStruct = sizeof(Line);

	IMAGEHLP_MODULE64 Module;
	memset(&Module, 0, sizeof(Module));
    Module.SizeOfStruct = sizeof(Module);

	EnumerateLoadedModules64(hProcess, LoadModuleCB, hProcess);

	std::stringstream callstackStr;

	bool bDone = false;
	const int MAX_DEPTH = 256;
	for(int iEntry = 0; (iEntry < MAX_DEPTH) && !bDone; ++iEntry)
	{
		bool bRet;
		
		bRet = StackWalk64(imageType, hProcess, hThread, &stackFrame, pContext, NULL, SymFunctionTableAccess64, SymGetModuleBase64, NULL) == TRUE;
		if(!bRet || stackFrame.AddrPC.Offset == 0)
		{
			bDone = true;
			continue;
		}

		DWORD64 dwDisplacementSymbol = 0;
		DWORD dwDisplacementLine = 0;

		bRet = SymGetSymFromAddr64(hProcess, stackFrame.AddrPC.Offset, &dwDisplacementSymbol, pSymbol) == TRUE;
		if(bRet)
		{
			char* pSymbolName = pSymbol->Name;

			DWORD dwRet = UnDecorateSymbolName(pSymbol->Name, undFullName, STACKWALK_MAX_NAMELEN, UNDNAME_COMPLETE);
			if(dwRet != 0)
				pSymbolName = undFullName;
			
			bool bHasLineInfo = SymGetLineFromAddr64(hProcess, stackFrame.AddrPC.Offset, &dwDisplacementLine, &Line) == TRUE;
			bool bHasModuleInfo = SymGetModuleInfo64(hProcess, stackFrame.AddrPC.Offset, &Module) == TRUE;

			if(iEntry == 0)
				callstackStr << "> ";
			else
				callstackStr << "  ";

			if(bHasModuleInfo)
				callstackStr << fs::path(Module.ImageName).filename();
			else
				callstackStr << "??";

			callstackStr << "!";
			callstackStr << pSymbolName;
			callstackStr << "() ";
			
			if(bHasLineInfo)
			{
				callstackStr << " Line ";
				callstackStr << Line.LineNumber;
			}

			if(dwDisplacementLine)
			{
				callstackStr << " + 0x";
				callstackStr << std::hex << dwDisplacementLine << std::dec;
				callstackStr << " bytes";
			}
			
			callstackStr << "\n";
		}
	}

	return callstackStr.str();
}

std::string GetRegisters(PCONTEXT pCtx)
{
	std::string registersStr;
	char buf[256];
		
#if defined(_WIN64)
	sprintf(buf, "  RAX:%016X  RBX:%016X  RCX:%016X  RDX:%016X  RSI:%016X  RDI:%016X\n", pCtx->Rax, pCtx->Rbx, pCtx->Rcx, pCtx->Rdx, pCtx->Rsi, pCtx->Rdi);
	registersStr += buf;

	sprintf(buf, "  CS:RIP:%04X:%08X\n", pCtx->SegCs, pCtx->Rip);
	registersStr += buf;

	sprintf(buf, "  SS:RSP:%04X:%016X  RBP:%016X\n", pCtx->SegSs, pCtx->Rsp, pCtx->Rbp);
	registersStr += buf;
#else
    sprintf(buf, "  EAX:%08X  EBX:%08X  ECX:%08X  EDX:%08X  ESI:%08X  EDI:%08X\n", pCtx->Eax, pCtx->Ebx, pCtx->Ecx, pCtx->Edx, pCtx->Esi, pCtx->Edi);
	registersStr += buf;

	sprintf(buf, "  CS:EIP:%04X:%08X\n", pCtx->SegCs, pCtx->Eip);
	registersStr += buf;

	sprintf(buf, "  SS:ESP:%04X:%08X  EBP:%08X\n", pCtx->SegSs, pCtx->Esp, pCtx->Ebp);
	registersStr += buf;
#endif

	sprintf(buf, "  DS:%04X  ES:%04X  FS:%04X  GS:%04X\n", pCtx->SegDs, pCtx->SegEs, pCtx->SegFs, pCtx->SegGs);
	registersStr += buf;

	sprintf(buf, "  Flags:%08X\n", pCtx->EFlags);
	registersStr += buf;

	return registersStr;
}

// Given an exception code, returns a pointer to a static string with a description of the exception                                         
std::string GetExceptionString( DWORD dwCode )
{
    #define EXCEPTION( x ) case EXCEPTION_##x: return (#x);

    switch ( dwCode )
    {
        EXCEPTION( ACCESS_VIOLATION )
        EXCEPTION( DATATYPE_MISALIGNMENT )
        EXCEPTION( BREAKPOINT )
        EXCEPTION( SINGLE_STEP )
        EXCEPTION( ARRAY_BOUNDS_EXCEEDED )
        EXCEPTION( FLT_DENORMAL_OPERAND )
        EXCEPTION( FLT_DIVIDE_BY_ZERO )
        EXCEPTION( FLT_INEXACT_RESULT )
        EXCEPTION( FLT_INVALID_OPERATION )
        EXCEPTION( FLT_OVERFLOW )
        EXCEPTION( FLT_STACK_CHECK )
        EXCEPTION( FLT_UNDERFLOW )
        EXCEPTION( INT_DIVIDE_BY_ZERO )
        EXCEPTION( INT_OVERFLOW )
        EXCEPTION( PRIV_INSTRUCTION )
        EXCEPTION( IN_PAGE_ERROR )
        EXCEPTION( ILLEGAL_INSTRUCTION )
        EXCEPTION( NONCONTINUABLE_EXCEPTION )
        EXCEPTION( STACK_OVERFLOW )
        EXCEPTION( INVALID_DISPOSITION )
        EXCEPTION( GUARD_PAGE )
        EXCEPTION( INVALID_HANDLE )
    }

    // If not one of the "known" exceptions, try to get the string
    // from NTDLL.DLL's message table.

    static char szBuffer[512] = { 0 };

    FormatMessageA( FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_FROM_HMODULE,
                    GetModuleHandleA( "NTDLL.DLL" ),
                    dwCode, 0, szBuffer, sizeof( szBuffer ), 0 );

    return szBuffer;
}
#endif

bool ErrorReport::GetMiscCrashInfo()
{
	// Copy the detailed description to an std::string for easier manipulation
	m_ReportDetailedDescription = m_pCrashInfo->detailedCrashInfo;

	// Get crash time
	m_CrashDateTime = QDateTime::currentDateTime();

#ifdef HAVE_WINAPI
	
	// Open parent process handle
	HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, m_pCrashInfo->processId);
	if(hProcess != NULL)
	{
		// Get memory usage info
		PROCESS_MEMORY_COUNTERS meminfo;
		BOOL bGetMemInfo = GetProcessMemoryInfo(hProcess, &meminfo, sizeof(PROCESS_MEMORY_COUNTERS));
		if(bGetMemInfo)
			m_ProcessMemoryUsage = meminfo.WorkingSetSize;

		// Determine the period of time the process is working.
		FILETIME CreationTime, ExitTime, KernelTime, UserTime;
		BOOL bGetTimes = GetProcessTimes(hProcess, &CreationTime, &ExitTime, &KernelTime, &UserTime);
		if(bGetTimes)
		{
			SYSTEMTIME AppStartTime;
			FileTimeToSystemTime(&CreationTime, &AppStartTime);

			SYSTEMTIME CurTime;
			GetSystemTime(&CurTime);
			ULONG64 uCurTime = ConvertSystemTimeToULONG64(CurTime);
			ULONG64 uStartTime = ConvertSystemTimeToULONG64(AppStartTime);

			// Check that the application works for at least one minute before crash.
			// This might help to avoid cyclic error report generation when the applciation
			// crashes on startup.
			m_RunningTimeSec = (double)(uCurTime-uStartTime)*10E-08;
		}
	}

	// Get operating system friendly name from registry.
	char OSNameBuf[256];
	if(!GetWindowsVersionName(OSNameBuf, 256))
		return false;
	m_OSName = OSNameBuf;

	// Determine if Windows is 64-bit.
	m_OSIs64Bit = Is64BitWindows();
	
	if(m_pCrashInfo->exceptionRecord.ExceptionCode != 0)
	{
		std::string exceptionStr = GetExceptionString(m_pCrashInfo->exceptionRecord.ExceptionCode);
		if(!exceptionStr.empty())
		{
			m_ReportDetailedDescription += "\nException code:\n  ";
			m_ReportDetailedDescription += exceptionStr + "\n";
		}
	}

	std::string callStack = GetCallStack(hProcess, m_pCrashInfo->threadHandle, &m_pCrashInfo->contextRecord);
	if(!callStack.empty())
	{
		m_ReportDetailedDescription += "\nCallstack:\n";
		m_ReportDetailedDescription += callStack;
	}

	std::string registers = GetRegisters(&m_pCrashInfo->contextRecord);
	if(!registers.empty())
	{
		m_ReportDetailedDescription += "\nRegisters:\n";
		m_ReportDetailedDescription += registers;
	}

	CloseHandle(hProcess);
#else
	
	// TODO
	
#endif

	return true;
}

bool ErrorReport::WriteReport(const fs::path& fileName)
{
	fs::path fullPath = m_ReportFolder / fileName;

	QFile file(fullPath.string().c_str());
	if(!file.open(QIODevice::WriteOnly))
		return false;

	AddFile(fullPath);

	QXmlStreamWriter xml;
	xml.setDevice(&file);
	xml.setAutoFormatting(true);
	xml.writeStartDocument();

		xml.writeComment("Information related to the crashed process");
		xml.writeStartElement("Process");
			xml.writeTextElement("Name", m_ProcessName);
			xml.writeTextElement("Path", m_ProcessPath.string().c_str());
			xml.writeTextElement("MemoryUsage", QString::number(m_ProcessMemoryUsage));
			xml.writeTextElement("Is64Bit", m_ProcessIs64Bit ? "True" : "False");
			xml.writeTextElement("RunningTime", QString::number(m_RunningTimeSec));
			xml.writeTextElement("CrashDateTime", m_CrashDateTime.toString("dd.MM.yyyy hh:mm:ss"));
		xml.writeEndElement();

		xml.writeComment("Information related to the OS");
		xml.writeStartElement("OS");
			xml.writeTextElement("Name", m_OSName);
			xml.writeTextElement("Is64Bit", m_OSIs64Bit ? "True" : "False");
		xml.writeEndElement();

		xml.writeComment("List of files generated by the crash reporter");
		xml.writeComment("Note that some of these files could have been manually excluded from the report");
		xml.writeStartElement("Files");
		for(FileList::const_iterator it = m_AttachedFiles.begin(); it != m_AttachedFiles.end(); ++it)
		{
			xml.writeTextElement("File", it->path.string().c_str());
		}
		xml.writeEndElement();

	xml.writeEndDocument();

	file.close();

	return true;
}

bool ErrorReport::GenerateArchive()
{
	std::string fileName = m_SharedMemoryName + ".zip";
	fs::path fullPath = m_ReportFolder / fileName;

	int bufferSize = 64*1024;
	void* buf = malloc(bufferSize);

	zipFile zf = zipOpen(fullPath.string().c_str(), 0);

	if(zf == 0) 
	{
		free(buf);
		return false;
	}

	int err = ZIP_OK;
	for(FileList::const_iterator it = m_AttachedFiles.begin(); it != m_AttachedFiles.end(); ++it) 
	{
		if(!it->attachToReport)
			continue;

		zip_fileinfo zi;
		memset(&zi, 0, sizeof(zi));

		int opt_compress_level = Z_BEST_COMPRESSION;

		// TODO - set real time
		//filetime(fn.toAscii(), &zi.dosDate);

		zipOpenNewFileInZip(zf, 
							it->path.filename().c_str(),
							&zi,
							NULL,
							0,
							NULL,
							0,
							NULL,
							(opt_compress_level != 0) ? Z_DEFLATED : 0,
							opt_compress_level);

		// Open source file
		FILE * fin = fopen(it->path.string().c_str(), "r+b");
		if(!fin)
		{
			err = ZIP_INTERNALERROR;
			break;
		}

		// Copy data from source file into the zip file
		int size_read;
		do
		{
			size_read = fread(buf,1,bufferSize,fin);
			if (size_read < bufferSize)
			{
				if (feof(fin)==0)
				{
					err = ZIP_INTERNALERROR;
					break;
				}
			}

			if (err == ZIP_OK && size_read>0)
			{
				err = zipWriteInFileInZip (zf,buf,size_read);
				if (err < 0)
				{
					break;
				}
			}
		}
		while (err==ZIP_OK && size_read>0);
		if(err != ZIP_OK)
			break;

		fclose(fin);

		err = zipCloseFileInZip(zf);
		if(err != ZIP_OK)
			break;
	}

	free(buf);
	
	if(err != ZIP_OK)
	{
		zipClose(zf, "Generated by Arx Libertatis crash reporter");
		return false;
	}

	err = zipClose(zf, "Generated by Arx Libertatis crash reporter");
	return err == ZIP_OK;
}

bool ErrorReport::GenerateReport(ErrorReport::IProgressNotifier* pProgressNotifier)
{
	ErrorReport* report = this;
	BOOST_SCOPE_EXIT((report))
	{
		// Allow the crashed process to exit
		report->ReleaseApplicationLock();
	} BOOST_SCOPE_EXIT_END

	pProgressNotifier->taskStarted("Generating crash report", 4);
	
	// Initialize shared memory
	pProgressNotifier->taskStepStarted("Connecting to crashed application");
	Initialize();
	pProgressNotifier->taskStepEnded();

	// Take screenshot - non critical
	pProgressNotifier->taskStepStarted("Grabbing screenshot");
	GetScreenshot("screenshot.jpg");
	pProgressNotifier->taskStepEnded();

	// Generate minidump
	pProgressNotifier->taskStepStarted("Generating crash dump");
	bool bCrashDump = GetCrashDump("crash.dmp");
	pProgressNotifier->taskStepEnded();
	if(!bCrashDump)
	{
		pProgressNotifier->setError("Could not generate the crash dump.");
		return false;
	}

	// Gather machine info - SLOW!
	//taskStepStarted("Gathering system information");
	//bool bMachineInfo = m_errorReport.GetMachineInfo("machineInfo.txt");
	//taskStepEnded();

	// Generate manifest
	pProgressNotifier->taskStepStarted("Generating report manifest");
	bool bCrashXml = WriteReport("crash.xml");
	pProgressNotifier->taskStepEnded();
	if(!bCrashXml)
	{
		pProgressNotifier->setError("Could not generate the manifest.");
		return false;
	}

	return true;
}

bool ErrorReport::SendReport(ErrorReport::IProgressNotifier* pProgressNotifier)
{
	pProgressNotifier->taskStarted("Sending crash report", 3);

	// Generate archive
	pProgressNotifier->taskStepStarted("Compressing report");
	bool bCrashArchive = GenerateArchive();
	pProgressNotifier->taskStepEnded();
	if(!bCrashArchive)
	{
		pProgressNotifier->setError("Could not generate the error archive.");
		return false;
	}

	std::string fileName = m_SharedMemoryName + ".zip";
	fs::path fullPath = m_ReportFolder / fileName.c_str();

	CSmtp smptClient;
	smptClient.SetSenderName("Arx Libertatis Crashes");
	smptClient.SetSenderMail("arxlibertatis.crashes@gmail.com");
	smptClient.AddRecipient("arxlibertatis.crashes@gmail.com");
	smptClient.SetSubject("Arx Libertatis Crash Report");
	smptClient.AddMsgLine(m_ReportDetailedDescription.c_str());
	smptClient.AddAttachment(fullPath.string().c_str());
	
	// Connect to server
	bool connected = false;
	pProgressNotifier->taskStepStarted("Connecting to server");
	{
		try
		{
			connected = smptClient.ConnectRemoteServer("smtp.gmail.com", 465, USE_SSL, true, "arxlibertatis.crashes@gmail.com", "yu8pnioo");
		}
		catch(const ECSmtp& err)
		{
			pProgressNotifier->setError(err.GetErrorText());
		}
	}
	pProgressNotifier->taskStepEnded();
	if(!connected)
		return false;

	// Send report
	bool sent = true;
	pProgressNotifier->taskStepStarted("Sending report");
	{
		try
		{
			smptClient.Send();
		}
		catch(const ECSmtp& err)
		{
			pProgressNotifier->setError(err.GetErrorText());
		}
	}
	pProgressNotifier->taskStepEnded();

	return sent;
}

void ErrorReport::ReleaseApplicationLock()
{
	m_pCrashInfo->exitLock.post();
}

void ErrorReport::AddFile(const fs::path& fileName)
{
	m_AttachedFiles.push_back(File(fileName, true));
}

ErrorReport::FileList& ErrorReport::GetAttachedFiles()
{
	return m_AttachedFiles;
}

const std::string& ErrorReport::GetErrorDescription() const
{
	return m_ReportDetailedDescription;
}
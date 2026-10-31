#include <ctime>
#include <cstdlib>
#include <cstring>
#include <csignal>
#include <cstdio>
#include <string>
#define TOD_DEBUG_IMPLEMENTATION
#include "TodDebug.h"
#include "TodCommon.h"
#include "misc/Debug.h"
#include "misc/SEHCatcher.h"
#include "SexyAppFramework/SexyAppBase.h"
#ifdef __APPLE__
#include "SexyAppFramework/platform/macos/MacSystem.h"
#endif

// 命名污染审计：以下仅本 TU 使用，声明自对应头文件下沉（内部链接，前置声明供前部引用）。
static void TodLogString(const char* theMsg);
#ifdef _WIN32
static void TodReportError(LPEXCEPTION_POINTERS exceptioninfo, const char* theMessage);
#endif


using namespace Sexy;

static std::string gLogFileName;

namespace
{
void TodDebugOutput(const char* message)
{
#ifdef _WIN32
	OutputDebugStringA(message);
#else
	std::fputs(message, stderr);
	std::fflush(stderr);
#endif
}

bool TodDebuggerPresent()
{
#if defined(__APPLE__)
    return MacDebuggerPresent();
#elif defined(_WIN32)
	return IsDebuggerPresent() != FALSE;
#else
	FILE* status = std::fopen("/proc/self/status", "r");
	if (!status)
		return false;
	char line[256];
	unsigned int tracer = 0;
	while (std::fgets(line, sizeof(line), status))
		if (std::sscanf(line, "TracerPid: %u", &tracer) == 1)
			break;
	std::fclose(status);
	return tracer != 0;
#endif
}
}

void TodAssertBreakIfDebuggerPresent()
{
	if (!TodDebuggerPresent())
		return;
#ifdef _WIN32
	__debugbreak();
#else
	std::raise(SIGTRAP);
#endif
}

// Keep va_end on both paths without adding a C++ destructor to the function
// containing the legacy MSVC __try/__except assertion report below.
static int TodVsnprintfAndEnd(char* buffer, int size, const char* format, va_list& args)
{
	try
	{
		const int count = TodVsnprintf(buffer, size, format, args);
		va_end(args);
		return count;
	}
	catch (...)
	{
		va_end(args);
		throw;
	}
}

//0x514EA0
void TodErrorMessageBox(const char* theMessage, const char* theTitle)
{
	// Error text can include a long resource path. Do not pass it through the
	// ordinary 1 KiB trace formatter after constructing the full diagnostic.
	std::string aLogMessage = std::string(theMessage) + "." + theTitle;
	if (aLogMessage.back() != '\n')
		aLogMessage.push_back('\n');
	TodDebugOutput(aLogMessage.c_str());
	TodLogString(aLogMessage.c_str());
#ifdef _WIN32
	HWND hWnd = (gSexyAppBase && gSexyAppBase->mHWnd) ? gSexyAppBase->mHWnd : GetActiveWindow();
	MessageBoxA(hWnd, theMessage, theTitle, MB_ICONEXCLAMATION);
#else
	if (gSexyAppBase)
		gSexyAppBase->MsgBox(theMessage, theTitle, MB_OK | MB_ICONERROR);
#endif
}

void TodTraceMemory()
{
}

void* TodMalloc(int theSize)
{
	TOD_ASSERT(theSize > 0);
	return malloc(theSize);
}

void TodFree(void* theBlock)
{
	if (theBlock != nullptr)
	{
		free(theBlock);
	}
}

void TodAssertFailed(const char* theCondition, const char* theFile, int theLine, const char* theMsg, ...)
{
	char aFormattedMsg[1024];
	va_list argList;
	va_start(argList, theMsg);
	int aCount = TodVsnprintfAndEnd(aFormattedMsg, sizeof(aFormattedMsg), theMsg, argList);

	if (aCount != 0) {
		if (aFormattedMsg[aCount - 1] != '\n')
		{
			if (aCount + 1 < 1024)
			{
				aFormattedMsg[aCount] = '\n';
				aFormattedMsg[aCount + 1] = '\0';
			}
			else
			{
				aFormattedMsg[aCount - 1] = '\n';
			}
		}
	}

	char aBuffer[1024];
	if (*theCondition != '\0')
	{
		TodSnprintf(aBuffer, sizeof(aBuffer), "\n%s(%d)\nassertion failed: '%s'\n%s\n", theFile, theLine, theCondition, aFormattedMsg);
	}
	else
	{
		TodSnprintf(aBuffer, sizeof(aBuffer), "\n%s(%d)\nassertion failed: %s\n", theFile, theLine, aFormattedMsg);
	}
	TodTrace("%s", aBuffer);

	if (!TodDebuggerPresent())
	{
		if (gInAssert)
		{
			TodLog("Assert during exception processing");
			exit(0);
		}


		gInAssert = true;
#ifdef _WIN32
		LPEXCEPTION_POINTERS exp;

		__try
		{
			RaiseException(EXCEPTION_NONCONTINUABLE_EXCEPTION, NULL, NULL, NULL);
		}
		__except (exp = GetExceptionInformation(), EXCEPTION_CONTINUE_EXECUTION)
		{
			TodReportError(exp, aFormattedMsg);
		}

		gInAssert = false;
		exit(0);
#else
		TodLog("%s", aBuffer);
		std::abort();
#endif
	}
}

void TodLog(const char* theFormat, ...)
{
	char aButter[1024];
	va_list argList;
	va_start(argList, theFormat);
	int aCount = TodVsnprintfAndEnd(aButter, sizeof(aButter), theFormat, argList);

	if (aCount == 0 || aButter[aCount - 1] != '\n')
	{
		if (aCount + 1 < 1024)
		{
			aButter[aCount] = '\n';
			aButter[aCount + 1] = '\0';
		}
		else
		{
			aButter[aCount - 1] = '\n';
		}
	}

	TodLogString(aButter);
}

static void TodLogString(const char* theMsg)
{
	FILE* f = fopen(gLogFileName.c_str(), "a");
	if (f == nullptr)
	{
		TodDebugOutput("Failed to open log file\n");
		return;
	}

	if (fwrite(theMsg, strlen(theMsg), 1, f) != 1)
	{
		TodDebugOutput("Failed to write to log file\n");
	}

	fclose(f);
}

void TodTrace(const char* theFormat, ...)
{
	char aButter[1024];
	va_list argList;
	va_start(argList, theFormat);
	int aCount = TodVsnprintfAndEnd(aButter, sizeof(aButter), theFormat, argList);

	if (aCount == 0 || aButter[aCount - 1] != '\n')
	{
		if (aCount + 1 < 1024)
		{
			aButter[aCount] = '\n';
			aButter[aCount + 1] = '\0';
		}
		else
		{
			aButter[aCount - 1] = '\n';
		}
	}

	TodDebugOutput(aButter);
}

void TodTraceAndLog(const char* theFormat, ...)
{
	char aButter[1024];
	va_list argList;
	va_start(argList, theFormat);
	int aCount = TodVsnprintfAndEnd(aButter, sizeof(aButter), theFormat, argList);

	if (aCount == 0 || aButter[aCount - 1] != '\n')
	{
		if (aCount + 1 < 1024)
		{
			aButter[aCount] = '\n';
			aButter[aCount + 1] = '\0';
		}
		else
		{
			aButter[aCount - 1] = '\n';
		}
	}

	TodDebugOutput(aButter);
	TodLogString(aButter);
}

void TodTraceWithoutSpamming(const char* theFormat, ...)
{
	static std::time_t gLastTraceTime = 0;
	const std::time_t aTime = std::time(nullptr);
	if (aTime <= gLastTraceTime)
	{
		return;
	}

	gLastTraceTime = aTime;
	char aButter[1024];
	va_list argList;
	va_start(argList, theFormat);
	int aCount = TodVsnprintfAndEnd(aButter, sizeof(aButter), theFormat, argList);

	if (aCount == 0 || aButter[aCount - 1] != '\n')
	{
		if (aCount + 1 < 1024)
		{
			aButter[aCount] = '\n';
			aButter[aCount + 1] = '\0';
		}
		else
		{
			aButter[aCount - 1] = '\n';
		}
	}

	TodDebugOutput(aButter);
}

#ifdef _WIN32
static void TodReportError(LPEXCEPTION_POINTERS exceptioninfo, const char* theMessage)
{
	(void)theMessage;
	Sexy::SEHCatcher::UnhandledExceptionFilter(exceptioninfo);
}

static long __stdcall TodUnhandledExceptionFilter(LPEXCEPTION_POINTERS exceptioninfo)
{
	if (gInAssert)
	{
		TodLog("Exception during exception processing");
	}
	else
	{
		gInAssert = true;
		TodLog("\nUnhandled Exception");
		TodReportError(exceptioninfo, "Unhandled Exception");
		gInAssert = false;
	}

	return EXCEPTION_EXECUTE_HANDLER;
}
#endif

void TodAssertInitForApp()
{
	// log.txt is an original game log.  1073 writes it directly below the
	// executable-local data tree; keep that behavior separate from remake
	// diagnostics, which use ./log.
	gLogFileName = Sexy::GetDataFilePath("log.txt");

#ifdef _WIN32
	SetUnhandledExceptionFilter(TodUnhandledExceptionFilter);
#endif
}


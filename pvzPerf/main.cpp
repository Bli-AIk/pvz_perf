#include "LawnApp.h"
#include "SexyAppFramework/misc/FloatingPointMode.h"
#include "Resources.h"
#include "Sexy.TodLib/TodStringFile.h"
#include "Sexy.TodLib/TodDebug.h"
#include "SexyAppFramework/misc/KeyCodes.h"
#include "SexyAppFramework/misc/SEHCatcher.h"
#include "SexyAppFramework/misc/ResourceError.h"
#include "SexyAppFramework/widget/WidgetManager.h"
#include <string>
#include <cstdio>
#include <exception>
#ifdef _WIN32
#include "SexyAppFramework/platform/windows/WindowApi.h"
#else
#include "SexyAppFramework/platform/ErrorDialog.h"
#endif
using namespace Sexy;

bool (*gAppCloseRequest)();				//[0x69E6A0]

namespace
{
void WriteFatalLog(const char* message)
{
	TodLog("%s\n", message);
	fprintf(stderr, "pvz_perf: %s\n", message);
}
bool HasRuntimeFile(const std::string& theDirectory, const char* theRelativePath)
{
	if (theDirectory.empty())
		return false;

	const std::string aPath = AddTrailingSlash(theDirectory, true) + theRelativePath;
	return Sexy::FileExists(aPath);
}

bool HasRuntimeRoot(const std::string& theDirectory)
{
	return HasRuntimeFile(theDirectory, "main.pak64");
}

std::string SelectRuntimeRoot(const std::string& theExecutableDirectory)
{
	if (HasRuntimeRoot(theExecutableDirectory))
		return theExecutableDirectory;

	const std::string aParentDirectory = GetFileDir(RemoveTrailingSlash(theExecutableDirectory));
	if (HasRuntimeRoot(aParentDirectory))
		return aParentDirectory;

	const std::string aCurrentDirectory = GetCurDir();
	if (HasRuntimeRoot(aCurrentDirectory))
		return aCurrentDirectory;

	// Preserve the executable-directory default so the eventual resource error
	// still points at the location the user launched.
	return theExecutableDirectory;
}
}

//0x44E8F0
#ifdef _WIN32
int WINAPI WinMain(_In_ HINSTANCE /* hInstance */, _In_opt_ HINSTANCE /* hPrevInstance */, _In_ LPSTR /* lpCmdLine */, _In_ int /* nCmdShow */)
#else
int main(int argc, char** argv)
#endif
{
	try {
    Sexy::EnableDenormalFlushForCurrentThread();
#ifdef _WIN32
    if (!NativeWindowsWindowApi::EnableSystemDpiAwareness())
        throw std::runtime_error("Cannot establish system DPI awareness");
#endif
	gTodStringFormats = gLawnStringFormats;
	gTodStringFormatCount = gLawnStringFormatCount;
	gAppCloseRequest = LawnGetCloseRequest;
	gLawnApp = new LawnApp();
	gLawnApp->mChangeDirTo = SelectRuntimeRoot(gLawnApp->mChangeDirTo);

#ifndef _WIN32
	// argv already separates arguments; values may contain spaces or '='.
	for (int index = 1; index < argc; ++index)
	{
		const std::string argument = argv[index];
		const auto equals = argument.find('=');
		gLawnApp->HandleCmdLineParam(argument.substr(0, equals),
			equals == std::string::npos ? "" : argument.substr(equals + 1));
	}
	gLawnApp->mCmdLineParsed = true;
#endif
        gLawnApp->Init();
        gLawnApp->Start();
        gLawnApp->Shutdown();
		gLawnApp->RethrowLoadingThreadError();
        delete gLawnApp;
        gLawnApp = nullptr;
        return 0;
    } catch (const ResourceManagerException& exception) {
        const auto& error=exception.error;
		const auto message=StrFormat("Resource loading failed.\nError code: %u\nPlan record: %u\nValue: %u\nByte offset: %zu",
			static_cast<unsigned>(error.reason),error.record,error.value,error.offset);
		WriteFatalLog(message.c_str());
#ifdef _WIN32
        MessageBoxA(nullptr,message.c_str(),"pvz_perf - Fatal error",MB_OK|MB_ICONERROR);
#else
        const auto details = message + "\n\nGame directory: " + GetCurDir();
        ShowNativeErrorDialog(details.c_str(), "pvz_perf - Fatal error");
#endif
        return 1;
    } catch (const std::exception& error) {
		WriteFatalLog(error.what());
#ifdef _WIN32
        MessageBoxA(nullptr,error.what(),"pvz_perf - Fatal error",MB_OK|MB_ICONERROR);
#else
        const auto details = std::string(error.what()) + "\n\nGame directory: " + GetCurDir();
        ShowNativeErrorDialog(details.c_str(), "pvz_perf - Fatal error");
#endif
        return 1;
    }
}

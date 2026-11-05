#include "ApplicationHost.h"
#include "Common.h"
#include "SexyAppBase.h"
#include "sound/BassMusicInterface.h"
#include "sound/DummyMusicInterface.h"
#include "Sexy.TodLib/TodCommon.h"
#include <thread>
#include <vector>
#include <stdexcept>
#ifdef __APPLE__
#include "macos/MacSystem.h"
#elif !defined(_WIN32)
#include <unistd.h>
#endif
using namespace Sexy;
#if defined(__ANDROID__)
namespace
{
std::string gAndroidRuntimeDirectory;
}
void Sexy::SetAndroidRuntimeDirectory(const std::string& thePath)
{
	gAndroidRuntimeDirectory = thePath;
}


#endif

std::string Sexy::GetExecutableDirectory()
{
#if defined(__APPLE__)
    return AddTrailingSlash(MacExecutableDirectory(), true);
#elif defined(_WIN32)
	std::vector<char> aPath(MAX_PATH, '\0');
	for (;;)
	{
		DWORD aLength = GetModuleFileNameA(nullptr, aPath.data(),
			static_cast<DWORD>(aPath.size()));
		if (aLength == 0)
			return GetCurDir();
		if (aLength < aPath.size() - 1)
		{
			std::string aDirectory(aPath.data(), aLength);
			size_t aSlash = aDirectory.find_last_of("\\/");
			if (aSlash == std::string::npos)
				return ".\\";
			aDirectory.erase(aSlash + 1);
			return aDirectory;
		}
		aPath.resize(aPath.size() * 2, '\0');
		if (aPath.size() > 32768)
			return GetCurDir();
	}
	#else
	#if defined(__ANDROID__)
	// /proc/self/exe 是 /system/bin/app_process64；注入目录优先。
	if (!gAndroidRuntimeDirectory.empty())
		return AddTrailingSlash(gAndroidRuntimeDirectory, true);
	#endif
	std::vector<char> path(1024);
	for (;;)
	{
		const auto length = readlink("/proc/self/exe", path.data(), path.size());
		if (length < 0)
			throw std::runtime_error("Cannot resolve executable via /proc/self/exe");
		if (static_cast<size_t>(length) < path.size())
			return GetFileDir(std::string(path.data(), length), true);
		path.resize(path.size() * 2);
	}
#endif
}

namespace Sexy {
void HostSleepFor(unsigned int milliseconds) {
#ifdef _WIN32
    Sleep(milliseconds);
#else
    std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
#endif
}
void HostSleepUntil(FrameDeadline::TimePoint deadline) { std::this_thread::sleep_until(deadline); }
void HostYield() {}
void HostStartLoading(SexyAppBase& app, void (*entry)(void*)) { app.mLoadingThread = std::thread(entry, &app); }
void HostFinishResourcePrewarm(SexyAppBase& app, const std::atomic<bool>& closeRequest) {
    while (!TodWarmDrained() && !app.mShutdown && !closeRequest)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
}
MusicInterface* HostCreateMusic(bool disabled, NativeWindowHandle window) {
    if (disabled) return new DummyMusicInterface;
    return new BassMusicInterface(window);
}
}

#include "Common.h"

#include "misc/MTRand.h"
#include "misc/Debug.h"
#ifdef _WIN32
#include <direct.h>
#include <io.h>
#include <aclapi.h>
#else
#include <unistd.h>
#include <filesystem>
#include <codecvt>
#include <locale>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <stdexcept>

#ifdef __APPLE__
#include "platform/macos/MacSystem.h"
#endif

HINSTANCE Sexy::gHInstance;
namespace Sexy
{
// 命名污染审计：仅本 TU 使用，声明自对应头文件下沉（内部链接）。
[[maybe_unused]] static int StrFindNoCase(const char* theStr, const char* theFind);
[[maybe_unused]] static bool StrPrefixNoCase(const char* theStr, const char* thePrefix, int maxLength = 10000000);
[[maybe_unused]] static SexyString CommaSeperate(int theValue);
[[maybe_unused]] static bool Deltree(const std::string& thePath);
static void SMemR(void*& _Src, void* _Dst, size_t _Size);
[[maybe_unused]] static void SMemRStr(void*& _Src, std::string& theString);
[[maybe_unused]] static void SMemWStr(void*& _Dst, const std::string& theString);
}

bool Sexy::gDebug = false;
static Sexy::MTRand gMTRand;
namespace Sexy
{
	std::string gAppDataFolder = "";
}

Sexy::NativeThreadId Sexy::CurrentNativeThreadIdentity() noexcept
{
#ifdef _WIN32
	return static_cast<NativeThreadId>(::GetCurrentThreadId());
#else
	return std::this_thread::get_id();
#endif
}

int Sexy::Rand()
{
	return gMTRand.Next();
}

int Sexy::Rand(int range)
{
	return gMTRand.Next(static_cast<unsigned int>(range));
}

float Sexy::Rand(float range)
{
	return gMTRand.Next(range);
}

void Sexy::SRand(unsigned int theSeed)
{
	gMTRand.SRand(theSeed);
}

void Sexy::GetRandState(unsigned int* theState, int& theIndex)
{
	gMTRand.GetState(theState, theIndex);
}

bool Sexy::CheckFor98Mill()
{
#ifdef _WIN32
	static bool needOsCheck = true;
	static bool is98Mill = false;

	if (needOsCheck)
	{
		// bool invalid = false; // unused
		OSVERSIONINFOEXA osvi;
		ZeroMemory(&osvi, sizeof(OSVERSIONINFOEXA));

		osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEXA);
		if( GetVersionExA((LPOSVERSIONINFOA)&osvi) == 0)
		{
			osvi.dwOSVersionInfoSize = sizeof (OSVERSIONINFOA);
			if ( GetVersionExA((LPOSVERSIONINFOA)&osvi) == 0)
				return false;
		}

		needOsCheck = false;
		is98Mill = osvi.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS; // let's check Win95, 98, *AND* ME.
	}

	return is98Mill;
#else
	return false;
#endif
}

bool Sexy::CheckForVista()
{
#ifdef _WIN32
	static bool needOsCheck = true;
	static bool isVista = false;

	if (needOsCheck)
	{
		// bool invalid = false; // unused
		OSVERSIONINFOEXA osvi;
		ZeroMemory(&osvi, sizeof(OSVERSIONINFOEXA));

		osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEXA);
		if( GetVersionExA((LPOSVERSIONINFOA)&osvi) == 0)
		{
			osvi.dwOSVersionInfoSize = sizeof (OSVERSIONINFOA);
			if ( GetVersionExA((LPOSVERSIONINFOA)&osvi) == 0)
				return false;
		}

		needOsCheck = false;
		isVista = osvi.dwMajorVersion >= 6;
	}

	return isVista;
#else
	return false;
#endif
}

std::string Sexy::GetAppDataFolder()
{
#ifndef _WIN32
	if (Sexy::gAppDataFolder.empty())
	{
		const char* overridePath = std::getenv("PVZ_APPDATA_DIR");
		const char* xdg = std::getenv("XDG_DATA_HOME");
		const char* home = std::getenv("HOME");
		std::string root;
		if (overridePath && *overridePath) root = overridePath;
		else if (xdg && *xdg && std::filesystem::path(xdg).is_absolute())
			root = std::string(xdg) + "/PlantsVsZombiesModern";
		else if (home && *home && std::filesystem::path(home).is_absolute())
			root = std::string(home) + "/.local/share/PlantsVsZombiesModern";
#if defined(__ANDROID__)
		// AndroidMain resolves the actual user/package directory through the
		// Activity, tests writability, then sets PVZ_APPDATA_DIR. Do not guess
		// a user ID or fall back to a retired package name here.
		else
			throw std::runtime_error("Android Activity has not initialized PVZ_APPDATA_DIR");
#else
		else throw std::runtime_error("Set PVZ_APPDATA_DIR or an absolute HOME/XDG_DATA_HOME");
#endif
		SetAppDataFolder(std::filesystem::absolute(root).lexically_normal().string());
	}
#endif
	return Sexy::gAppDataFolder;
}

std::string Sexy::GetLogFilePath(const std::string& theFileName)
{
	// The output directory is immutable launch configuration.
	static const std::string aLogDirectory = [] {
		const char* directory=getenv("PVZ_LOG_DIR");
#ifdef _WIN32
		auto path=directory?AddTrailingSlash(directory,true):GetExecutableDirectory()+"log\\";
#else
		auto path=directory && *directory ?
			AddTrailingSlash(std::filesystem::absolute(directory).lexically_normal().string()) :
			GetAppDataFolder()+"log/";
#endif
		MkDir(path);
		return path;
	}();
	return aLogDirectory + theFileName;
}

const char* Sexy::GetLogFilePathC(const char* theFileName)
{
    static thread_local std::string path;
    path=GetLogFilePath(theFileName?theFileName:"log.txt");
    return path.c_str();
}

void Sexy::SetAppDataFolder(const std::string& thePath)
{
#ifdef _WIN32
	if (CheckForVista())
	{
		std::string aPath = thePath;
		if (!aPath.empty())
		{
			if (aPath[aPath.length()-1] != '\\' && aPath[aPath.length()-1] != '/')
				aPath += '\\';
		}

		Sexy::gAppDataFolder = aPath;
	}
#else
	if (thePath.empty())
		throw std::invalid_argument("Linux app data root must not be empty");
	std::string nativePath = thePath;
	std::replace(nativePath.begin(), nativePath.end(), '\\', '/');
	Sexy::gAppDataFolder = AddTrailingSlash(
		std::filesystem::absolute(nativePath).lexically_normal().string());
	MkDir(Sexy::gAppDataFolder);
#endif
}


std::string Sexy::StringToUpper(const std::string& theString)
{
	std::string aString;

	for (unsigned i = 0; i < theString.length(); i++)
		aString += toupper(theString[i]);

	return aString;
}

std::wstring Sexy::StringToUpper(const std::wstring& theString)
{
	std::wstring aString;

	for (unsigned i = 0; i < theString.length(); i++)
		aString += towupper(theString[i]);

	return aString;
}

std::string Sexy::StringToLower(const std::string& theString)
{
	std::string aString;

	for (unsigned i = 0; i < theString.length(); i++)
		aString += tolower(theString[i]);

	return aString;
}

std::wstring Sexy::StringToLower(const std::wstring& theString)
{
	std::wstring aString;

	for (unsigned i = 0; i < theString.length(); ++i)
		aString += tolower(theString[i]);

	return aString;
}

std::wstring Sexy::StringToWString(const std::string &theString)
{
	std::wstring aString;
	aString.reserve(theString.length());
	for(size_t i = 0; i < theString.length(); ++i)
		aString += (unsigned char)theString[i];
	return aString;
}

std::string Sexy::WStringToString(const std::wstring &theString)
{
	size_t aRequiredLength = wcstombs( NULL, theString.c_str(), 0 );
	if (aRequiredLength < 16384)
	{
		char aBuffer[16384];
		wcstombs( aBuffer, theString.c_str(), 16384 );
		return std::string(aBuffer);
	}
	else
	{
		DBG_ASSERTE(aRequiredLength != (size_t)-1);
		if (aRequiredLength == (size_t)-1) return "";

		char* aBuffer = new char[aRequiredLength+1];
		wcstombs( aBuffer, theString.c_str(), aRequiredLength+1 );
		std::string aStr = aBuffer;
		delete[] aBuffer;
		return aStr;
	}
}

SexyString Sexy::StringToSexyString(const std::string& theString)
{
#ifdef _USE_WIDE_STRING
	return StringToWString(theString);
#else
	return SexyString(theString);
#endif
}

SexyString Sexy::WStringToSexyString(const std::wstring &theString)
{
#ifdef _USE_WIDE_STRING
	return SexyString(theString);
#else
	return WStringToString(theString);
#endif
}

std::string Sexy::SexyStringToString(const SexyString& theString)
{
#ifdef _USE_WIDE_STRING
	return WStringToString(theString);
#else
	return std::string(theString);
#endif
}

std::wstring Sexy::SexyStringToWString(const SexyString& theString)
{
#ifdef _USE_WIDE_STRING
	return std::wstring(theString);
#else
	return StringToWString(theString);
#endif
}

std::string Sexy::Trim(const std::string& theString)
{
	int aStartPos = 0;
	while ( aStartPos < (int) theString.length() && isspace((unsigned char)theString[aStartPos]) )
		aStartPos++;

	int anEndPos = theString.length() - 1;
	while ( anEndPos >= 0 && isspace((unsigned char)theString[anEndPos]) )
		anEndPos--;

	return theString.substr(aStartPos, anEndPos - aStartPos + 1);
}

std::wstring Sexy::Trim(const std::wstring& theString)
{   //0x5AFD80
	int aStartPos = 0;
	while ( aStartPos < (int) theString.length() && iswspace(theString[aStartPos]) )
		aStartPos++;

	int anEndPos = theString.length() - 1;
	while ( anEndPos >= 0 && iswspace(theString[anEndPos]) )
		anEndPos--;

	return theString.substr(aStartPos, anEndPos - aStartPos + 1);
}

bool Sexy::StringToInt(const std::string & theString, int* theIntVal)
{
	*theIntVal = 0;

	if (theString.length() == 0)
		return false;

	int theRadix = 10;
	bool isNeg = false;

	unsigned i = 0;
	if (theString[i] == '-')
	{
		isNeg = true;
		i++;
	}

	for (; i < theString.length(); i++)
	{
		char aChar = theString[i];
		
		if ((theRadix == 10) && (aChar >= '0') && (aChar <= '9'))
			*theIntVal = (*theIntVal * 10) + (aChar - '0');
		else if ((theRadix == 0x10) && 
			(((aChar >= '0') && (aChar <= '9')) || 
			 ((aChar >= 'A') && (aChar <= 'F')) || 
			 ((aChar >= 'a') && (aChar <= 'f'))))
		{			
			if (aChar <= '9')
				*theIntVal = (*theIntVal * 0x10) + (aChar - '0');
			else if (aChar <= 'F')
				*theIntVal = (*theIntVal * 0x10) + (aChar - 'A') + 0x0A;
			else
				*theIntVal = (*theIntVal * 0x10) + (aChar - 'a') + 0x0A;
		}
		else if (((aChar == 'x') || (aChar == 'X')) && (i == 1) && (*theIntVal == 0))
		{
			theRadix = 0x10;
		}
		else
		{
			*theIntVal = 0;
			return false;
		}
	}

	if (isNeg)
		*theIntVal = -*theIntVal;

	return true;
}

bool Sexy::StringToInt(const std::wstring & theString, int* theIntVal)
{
	*theIntVal = 0;

	if (theString.length() == 0)
		return false;

	int theRadix = 10;
	bool isNeg = false;

	unsigned i = 0;
	if (theString[i] == '-')
	{
		isNeg = true;
		i++;
	}

	for (; i < theString.length(); i++)
	{
		wchar_t aChar = theString[i];
		
		if ((theRadix == 10) && (aChar >= L'0') && (aChar <= L'9'))
			*theIntVal = (*theIntVal * 10) + (aChar - L'0');
		else if ((theRadix == 0x10) && 
			(((aChar >= L'0') && (aChar <= L'9')) || 
			 ((aChar >= L'A') && (aChar <= L'F')) || 
			 ((aChar >= L'a') && (aChar <= L'f'))))
		{			
			if (aChar <= L'9')
				*theIntVal = (*theIntVal * 0x10) + (aChar - L'0');
			else if (aChar <= L'F')
				*theIntVal = (*theIntVal * 0x10) + (aChar - L'A') + 0x0A;
			else
				*theIntVal = (*theIntVal * 0x10) + (aChar - L'a') + 0x0A;
		}
		else if (((aChar == L'x') || (aChar == L'X')) && (i == 1) && (*theIntVal == 0))
		{
			theRadix = 0x10;
		}
		else
		{
			*theIntVal = 0;
			return false;
		}
	}

	if (isNeg)
		*theIntVal = -*theIntVal;

	return true;
}

bool Sexy::StringToDouble(const std::string & theString, double* theDoubleVal)
{
	*theDoubleVal = 0.0;

	if (theString.length() == 0)
		return false;

	bool isNeg = false;

	unsigned i = 0;
	if (theString[i] == '-')
	{
		isNeg = true;
		i++;
	}

	for (; i < theString.length(); i++)
	{
		char aChar = theString[i];

		if ((aChar >= '0') && (aChar <= '9'))
			*theDoubleVal = (*theDoubleVal * 10) + (aChar - '0');
		else if (aChar == '.')
		{
			i++;
			break;
		}
		else
		{
			*theDoubleVal = 0.0;
			return false;
		}
	}

	double aMult = 0.1;
	for (; i < theString.length(); i++)
	{
		char aChar = theString[i];

		if ((aChar >= '0') && (aChar <= '9'))
		{
			*theDoubleVal += (aChar - '0') * aMult;	
			aMult /= 10.0;
		}
		else
		{
			*theDoubleVal = 0.0;
			return false;
		}
	}

	if (isNeg)
		*theDoubleVal = -*theDoubleVal;

	return true;
}

bool Sexy::StringToDouble(const std::wstring & theString, double* theDoubleVal)
{
	*theDoubleVal = 0.0;

	if (theString.length() == 0)
		return false;

	bool isNeg = false;

	unsigned i = 0;
	if (theString[i] == '-')
	{
		isNeg = true;
		i++;
	}

	for (; i < theString.length(); i++)
	{
		wchar_t aChar = theString[i];

		if ((aChar >= L'0') && (aChar <= L'9'))
			*theDoubleVal = (*theDoubleVal * 10) + (aChar - L'0');
		else if (aChar == L'.')
		{
			i++;
			break;
		}
		else
		{
			*theDoubleVal = 0.0;
			return false;
		}
	}

	double aMult = 0.1;
	for (; i < theString.length(); i++)
	{
		wchar_t aChar = theString[i];

		if ((aChar >= L'0') && (aChar <= L'9'))
		{
			*theDoubleVal += (aChar - L'0') * aMult;	
			aMult /= 10.0;
		}
		else
		{
			*theDoubleVal = 0.0;
			return false;
		}
	}

	if (isNeg)
		*theDoubleVal = -*theDoubleVal;

	return true;
}

// TODO: Use <locale> for localization of number output?
SexyString Sexy::CommaSeperate(int theValue)
{	
	if (theValue == 0)
		return ("0");

	SexyString aCurString;

	int aPlace = 0;
	int aCurValue = theValue;

	while (aCurValue > 0)
	{
		if ((aPlace != 0) && (aPlace % 3 == 0))
			aCurString = (',') + aCurString;
		aCurString = (SexyChar) (('0') + (aCurValue % 10)) + aCurString;
		aCurValue /= 10;
		aPlace++;
	}

	return aCurString;
}

std::string Sexy::GetCurDir()
{
#ifdef _WIN32
	char aDir[256];
	return _getcwd(aDir, sizeof(aDir));
#else
	return std::filesystem::current_path().string();
#endif
}

std::string Sexy::GetFullPath(const std::string& theRelPath)
{
	return GetPathFrom(theRelPath, GetCurDir());
}

std::string Sexy::GetDataFilePath(const std::string& theFileName)
{
#ifdef _WIN32
	std::string aDataDirectory = GetExecutableDirectory() + "data\\";
#else
	std::string aDataDirectory = GetAppDataFolder();
#endif
	MkDir(aDataDirectory);
	return aDataDirectory + theFileName;
}

std::string Sexy::GetPathFrom(const std::string& theRelPath, const std::string& theDir)
{
#ifndef _WIN32
	std::string relative = theRelPath, directory = theDir;
	std::replace(relative.begin(), relative.end(), '\\', '/');
	std::replace(directory.begin(), directory.end(), '\\', '/');
	return (std::filesystem::path(directory) / relative).lexically_normal().string();
#else
	std::string aDriveString;
	std::string aNewPath = theDir;

	if ((theRelPath.length() >= 2) && (theRelPath[1] == ':'))
		return theRelPath;

	char aSlashChar = '/';

	if ((theRelPath.find('\\') != (size_t)-1) || (theDir.find('\\') != (size_t)-1))
		aSlashChar = '\\';	

	if ((aNewPath.length() >= 2) && (aNewPath[1] == ':'))
	{
		aDriveString = aNewPath.substr(0, 2);
		aNewPath.erase(aNewPath.begin(), aNewPath.begin()+2);
	}

	// Append a trailing slash if necessary
	if ((aNewPath.length() > 0) && (aNewPath[aNewPath.length()-1] != '\\') && (aNewPath[aNewPath.length()-1] != '/'))
		aNewPath += aSlashChar;

	std::string aTempRelPath = theRelPath;

	for (;;)
	{
		if (aNewPath.length() == 0)
			break;

		int aFirstSlash = aTempRelPath.find('\\');
		int aFirstForwardSlash = aTempRelPath.find('/');

		if ((aFirstSlash == -1) || ((aFirstForwardSlash != -1) && (aFirstForwardSlash < aFirstSlash)))
			aFirstSlash = aFirstForwardSlash;

		if (aFirstSlash == -1)
			break;

		std::string aChDir = aTempRelPath.substr(0, aFirstSlash);

		aTempRelPath.erase(aTempRelPath.begin(), aTempRelPath.begin() + aFirstSlash + 1);						

		if (aChDir.compare("..") == 0)
		{			
			int aLastDirStart = aNewPath.length() - 1;
			while ((aLastDirStart > 0) && (aNewPath[aLastDirStart-1] != '\\') && (aNewPath[aLastDirStart-1] != '/'))
				aLastDirStart--;

			std::string aLastDir = aNewPath.substr(aLastDirStart, aNewPath.length() - aLastDirStart - 1);
			if (aLastDir.compare("..") == 0)
			{
				aNewPath += "..";
				aNewPath += aSlashChar;
			}
			else
			{
				aNewPath.erase(aNewPath.begin() + aLastDirStart, aNewPath.end());
			}
		}		
		else if (aChDir.compare("") == 0)
		{
			aNewPath = aSlashChar;
			break;
		}
		else if (aChDir.compare(".") != 0)
		{
			aNewPath += aChDir + aSlashChar;
			break;
		}
	}

	aNewPath = aDriveString + aNewPath + aTempRelPath;

	if (aSlashChar == '/')
	{
		for (;;)
		{
			int aSlashPos = aNewPath.find('\\');
			if (aSlashPos == -1)
				break;
			aNewPath[aSlashPos] = '/';
		}
	}
	else
	{
		for (;;)
		{
			int aSlashPos = aNewPath.find('/');
			if (aSlashPos == -1)
				break;
			aNewPath[aSlashPos] = '\\';
		}
	}

	return aNewPath;
#endif
}

bool Sexy::AllowAllAccess(const std::string& theFileName)
{
#ifdef _WIN32
	HMODULE aLib = LoadLibraryA("advapi32.dll");
	if (aLib == NULL)
		return false;

	BOOL (WINAPI *fnSetFileSecurity)(LPCTSTR lpFileName, SECURITY_INFORMATION SecurityInformation, PSECURITY_DESCRIPTOR pSecurityDescriptor);
	BOOL (WINAPI *fnSetSecurityDescriptorDacl)(PSECURITY_DESCRIPTOR pSecurityDescriptor, BOOL bDaclPresent, PACL pDacl, BOOL bDaclDefaulted);
	BOOL (WINAPI *fnInitializeSecurityDescriptor)(PSECURITY_DESCRIPTOR pSecurityDescriptor, DWORD dwRevision);
	BOOL (WINAPI *fnAllocateAndInitializeSid)(
	  PSID_IDENTIFIER_AUTHORITY pIdentifierAuthority,
	  BYTE nSubAuthorityCount,
	  DWORD dwSubAuthority0,
	  DWORD dwSubAuthority1,
	  DWORD dwSubAuthority2,
	  DWORD dwSubAuthority3,
	  DWORD dwSubAuthority4,
	  DWORD dwSubAuthority5,
	  DWORD dwSubAuthority6,
	  DWORD dwSubAuthority7,
	  PSID* pSid
	);
	DWORD (WINAPI *fnSetEntriesInAcl)(ULONG cCountOfExplicitEntries, PEXPLICIT_ACCESS pListOfExplicitEntries, PACL OldAcl, PACL* NewAcl);
	PVOID (WINAPI *fnFreeSid)(PSID pSid);

	*(void**)&fnSetFileSecurity = (void*)GetProcAddress(aLib, "SetFileSecurityA");
	*(void**)&fnSetSecurityDescriptorDacl = (void*)GetProcAddress(aLib, "SetSecurityDescriptorDacl");
	*(void**)&fnInitializeSecurityDescriptor = (void*)GetProcAddress(aLib, "InitializeSecurityDescriptor");
	*(void**)&fnAllocateAndInitializeSid = (void*)GetProcAddress(aLib, "AllocateAndInitializeSid");
	*(void**)&fnSetEntriesInAcl = (void*)GetProcAddress(aLib, "SetEntriesInAclA");
	*(void**)&fnFreeSid = (void*) GetProcAddress(aLib, "FreeSid");

	if (!(fnSetFileSecurity && fnSetSecurityDescriptorDacl && fnInitializeSecurityDescriptor && fnAllocateAndInitializeSid && fnSetEntriesInAcl && fnFreeSid))
	{
		FreeLibrary(aLib);
		return false;
	}


	PSID pEveryoneSID = NULL;
	SID_IDENTIFIER_AUTHORITY SIDAuthWorld = SECURITY_WORLD_SID_AUTHORITY;
	bool result = false;

    // Create a well-known SID for the Everyone group.
    if (fnAllocateAndInitializeSid(&SIDAuthWorld, 1,
                     SECURITY_WORLD_RID,
                     0, 0, 0, 0, 0, 0, 0,
                     &pEveryoneSID))
    {
		EXPLICIT_ACCESS ea;

		// Initialize an EXPLICIT_ACCESS structure for an ACE.
		// The ACE will allow Everyone read access to the key.
		ZeroMemory(&ea, sizeof(EXPLICIT_ACCESS));
		ea.grfAccessPermissions = STANDARD_RIGHTS_ALL | SPECIFIC_RIGHTS_ALL;
		ea.grfAccessMode = SET_ACCESS;
		ea.grfInheritance = SUB_CONTAINERS_AND_OBJECTS_INHERIT;
		ea.Trustee.TrusteeForm = TRUSTEE_IS_SID;
		ea.Trustee.TrusteeType = TRUSTEE_IS_WELL_KNOWN_GROUP;
		ea.Trustee.ptstrName = (LPTSTR) pEveryoneSID;

		// Create a new ACL that contains the new ACEs.
		PACL pACL = NULL; 
		if (fnSetEntriesInAcl(1, &ea, NULL, &pACL) == ERROR_SUCCESS)
		{		
			// Initialize a security descriptor.  
			PSECURITY_DESCRIPTOR pSD = (PSECURITY_DESCRIPTOR) new char[SECURITY_DESCRIPTOR_MIN_LENGTH]; 
						 
			if (fnInitializeSecurityDescriptor(pSD, SECURITY_DESCRIPTOR_REVISION)) 
			{  							 
				// Add the ACL to the security descriptor. 
				if (fnSetSecurityDescriptorDacl(pSD, 
						TRUE,     // bDaclPresent flag   
						pACL, 
						FALSE))   // not a default DACL 
				{
					if (fnSetFileSecurity(theFileName.c_str(), DACL_SECURITY_INFORMATION, pSD))
						result = true;
				}
			}
		}
	}

	FreeLibrary(aLib);
	return result;
#else
	// Per-user Linux files keep their existing permissions and umask.
	return access(theFileName.c_str(), R_OK | W_OK) == 0;
#endif
}

bool Sexy::Deltree(const std::string& thePath)
{
#ifdef _WIN32
	bool success = true;

	std::string aSourceDir = thePath;
	
	if (aSourceDir.length() < 2)
		return false;

	if ((aSourceDir[aSourceDir.length() - 1] != '\\') ||
		(aSourceDir[aSourceDir.length() - 1] != '/'))
		aSourceDir += "\\";		
	
	WIN32_FIND_DATAA aFindData;

	HANDLE aFindHandle = FindFirstFileA((aSourceDir + "*.*").c_str(), &aFindData); 
	if (aFindHandle == INVALID_HANDLE_VALUE)
		return false;
	
	do
	{		
		if ((aFindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
		{
			if ((strcmp(aFindData.cFileName, ".") != 0) &&
				(strcmp(aFindData.cFileName, "..") != 0))
			{
				// Follow the directory
				if (!Deltree(aSourceDir + aFindData.cFileName))
					success = false;
			}
		}
		else
		{	
			std::string aFullName = aSourceDir + aFindData.cFileName;
			if (!DeleteFileA(aFullName.c_str()))
				success = false;
		}
	}
	while (FindNextFileA(aFindHandle, &aFindData));
	FindClose(aFindHandle);

	if (rmdir(thePath.c_str()) == 0)
		success = false;

	return success;
#else
	// Historical private helper has no callers.
	std::error_code error;
	const auto path = std::filesystem::absolute(thePath).lexically_normal();
	if (path == path.root_path() || path == std::filesystem::current_path())
		return false;
	std::filesystem::remove_all(path, error);
	return !error;
#endif
}

bool Sexy::FileExists(const std::string& theFileName)
{
#ifdef _WIN32
	WIN32_FIND_DATAA aFindData;
	
	HANDLE aFindHandle = FindFirstFileA(theFileName.c_str(), &aFindData); 
	if (aFindHandle == INVALID_HANDLE_VALUE)
		return false;

	FindClose(aFindHandle);
	return true;
#else
	std::error_code error;
	std::string path = theFileName;
	std::replace(path.begin(), path.end(), '\\', '/');
	return std::filesystem::exists(path, error);
#endif
}

void Sexy::MkDir(const std::string& theDir)
{
#ifdef _WIN32
	std::string aPath = theDir;

	int aCurPos = 0;
	for (;;)
	{
		int aSlashPos = aPath.find_first_of("\\/", aCurPos);
		if (aSlashPos == -1)
		{
			_mkdir(aPath.c_str());
			break;
		}

		aCurPos = aSlashPos+1;

		std::string aCurPath = aPath.substr(0, aSlashPos);
		_mkdir(aCurPath.c_str());
	}
#else
	if (theDir.empty()) return;
	std::string path = theDir;
	std::replace(path.begin(), path.end(), '\\', '/');
	std::filesystem::create_directories(path);
#endif
}

std::string Sexy::GetFileName(const std::string& thePath, bool noExtension)
{
	int aLastSlash = std::max((int) thePath.rfind('\\'), (int) thePath.rfind('/'));

	if (noExtension)
	{
		int aLastDot = (int)thePath.rfind('.');
		if (aLastDot > aLastSlash)
			return thePath.substr(aLastSlash + 1, aLastDot - aLastSlash - 1);
	}

	if (aLastSlash == -1)
		return thePath;
	else
		return thePath.substr(aLastSlash + 1);
}

std::string Sexy::GetFileDir(const std::string& thePath, bool withSlash)
{
	int aLastSlash = std::max((int) thePath.rfind('\\'), (int) thePath.rfind('/'));

	if (aLastSlash == -1)
		return "";
	else
	{
		if (withSlash)
			return thePath.substr(0, aLastSlash+1);
		else
			return thePath.substr(0, aLastSlash);
	}
}

std::string Sexy::RemoveTrailingSlash(const std::string& theDirectory)
{
	int aLen = theDirectory.length();
	
	if ((aLen > 0) && ((theDirectory[aLen-1] == '\\') || (theDirectory[aLen-1] == '/')))
		return theDirectory.substr(0, aLen - 1);
	else
		return theDirectory;
}

std::string	Sexy::AddTrailingSlash(const std::string& theDirectory, bool backSlash)
{
#ifndef _WIN32
	backSlash = false;
#endif
	if (!theDirectory.empty())
	{
		char aChar = theDirectory[theDirectory.length()-1];
		if (aChar!='\\' && aChar!='/')
			return theDirectory + (backSlash?'\\':'/');
		else
			return theDirectory;
	}
	else
		return "";
}


time_t Sexy::GetFileDate(const std::string& theFileName)
{
#ifdef _WIN32
	time_t aFileDate = 0;

	WIN32_FIND_DATAA aFindData;
	HANDLE aFindHandle = ::FindFirstFileA(theFileName.c_str(), &aFindData);

	if (aFindHandle != INVALID_HANDLE_VALUE)
	{		
		FILETIME aFileTime = aFindData.ftLastWriteTime;
						
		//FileTimeToUnixTime(&aFileTime, &aFileDate, FALSE);

		LONGLONG ll = (__int64) aFileTime.dwHighDateTime << 32;
		ll = ll + aFileTime.dwLowDateTime - 116444736000000000;
		aFileDate = (time_t) (ll/10000000);

		FindClose(aFindHandle);
	}

	return aFileDate;
#else
	struct stat info{};
	std::string path = theFileName;
	std::replace(path.begin(), path.end(), '\\', '/');
	return stat(path.c_str(), &info) == 0 ? info.st_mtime : 0;
#endif
}

std::string Sexy::vformat(const char* fmt, va_list argPtr)
{
	char stackBuffer[161];
	va_list copy;
	va_copy(copy, argPtr);
	const int required = std::vsnprintf(stackBuffer, sizeof(stackBuffer), fmt, copy);
	va_end(copy);
	if (required < 0)
		throw std::runtime_error("StrFormat: narrow printf encoding or format error");
	if (static_cast<size_t>(required) < sizeof(stackBuffer))
		return std::string(stackBuffer);

	// Size counts characters, not bytes. Own the allocation across formatting
	// and result construction; a failed allocation must propagate unchanged.
	std::string result(static_cast<size_t>(required), '\0');
	va_copy(copy, argPtr);
	const int written = std::vsnprintf(result.data(), result.size() + 1, fmt, copy);
	va_end(copy);
	if (written != required)
		throw std::runtime_error("StrFormat: narrow printf output changed between passes");
	// Preserve the original C-string result for embedded NUL characters.
	result.resize(std::strlen(result.c_str()));
	return result;
}

std::string Sexy::StrFormat(const char* fmt ...)
{
	va_list args;
	va_start(args, fmt);
	try
	{
		auto result = vformat(fmt, args);
		va_end(args);
		return result;
	}
	catch (...)
	{
		va_end(args);
		throw;
	}
}

std::wstring Sexy::vformat(const wchar_t* fmt, va_list argPtr)
{
	wchar_t stackBuffer[161];
	va_list copy;
	va_copy(copy, argPtr);
	errno = 0;
	const int count = std::vswprintf(stackBuffer, LENGTH(stackBuffer), fmt, copy);
	const int formatError = errno;
	va_end(copy);
	if (count >= 0 && static_cast<size_t>(count) < LENGTH(stackBuffer))
		return std::wstring(stackBuffer);
	if (formatError == EILSEQ || formatError == EINVAL)
		throw std::runtime_error("StrFormat: wide printf encoding or format error");

#ifdef _WIN32
	// The Windows CRT can measure wide printf output without truncation.
	va_copy(copy, argPtr);
	const int required = _vscwprintf(fmt, copy);
	va_end(copy);
	if (required < 0)
		throw std::runtime_error("StrFormat: cannot measure wide printf output");
	std::wstring result(static_cast<size_t>(required), L'\0');
	va_copy(copy, argPtr);
	const int written = std::vswprintf(result.data(), result.size() + 1, fmt, copy);
	va_end(copy);
	if (written != required)
		throw std::runtime_error("StrFormat: wide printf output changed between passes");
	result.resize(std::wcslen(result.c_str()));
	return result;
#else
	// ISO/POSIX vswprintf does not promise a sizing-only call. Retry with
	// typed storage and a fresh va_list, never with a byte-sized realloc.
	std::wstring result(320, L'\0');
	for (;;)
	{
		va_copy(copy, argPtr);
		errno = 0;
		const int written = std::vswprintf(result.data(), result.size() + 1, fmt, copy);
		const int error = errno;
		va_end(copy);
		if (written >= 0 && static_cast<size_t>(written) <= result.size())
		{
			result.resize(std::wcslen(result.c_str()));
			return result;
		}
		if (error == EILSEQ || error == EINVAL)
			throw std::runtime_error("StrFormat: wide printf encoding or format error");
		if (result.size() > result.max_size() / 2)
			throw std::length_error("StrFormat: wide printf output exceeds string capacity");
		result.resize(written >= 0 ? static_cast<size_t>(written) : result.size() * 2);
	}
#endif
}

std::wstring Sexy::StrFormat(const wchar_t* fmt ...)
{
	va_list args;
	va_start(args, fmt);
	try
	{
		auto result = vformat(fmt, args);
		va_end(args);
		return result;
	}
	catch (...)
	{
		va_end(args);
		throw;
	}
}

std::string Sexy::Evaluate(const std::string& theString, const DefinesMap& theDefinesMap)
{
	std::string anEvaluatedString = theString;

	for (;;)
	{
		size_t aPercentPos = anEvaluatedString.find('%');

		if (aPercentPos == std::string::npos)
			break;
		
		size_t aSecondPercentPos = anEvaluatedString.find('%', aPercentPos + 1);
		if (aSecondPercentPos == std::string::npos)
			break;

		std::string aName = anEvaluatedString.substr(aPercentPos + 1, aSecondPercentPos - aPercentPos - 1);

		std::string aValue;
		DefinesMap::const_iterator anItr = theDefinesMap.find(aName);		
		if (anItr != theDefinesMap.end())
			aValue = anItr->second;
		else
			aValue = "";		

		anEvaluatedString.erase(anEvaluatedString.begin() + aPercentPos, anEvaluatedString.begin() + aSecondPercentPos + 1);
		anEvaluatedString.insert(anEvaluatedString.begin() + aPercentPos, aValue.begin(), aValue.begin() + aValue.length());
	}

	return anEvaluatedString;
}

std::string Sexy::Upper(const std::string& _data)
{
	std::string s = _data;
	std::transform(s.begin(), s.end(), s.begin(), toupper);
	return s;
}

std::wstring Sexy::Upper(const std::wstring& _data)
{
	std::wstring s = _data;
	std::transform(s.begin(), s.end(), s.begin(), towupper);
	return s;
}

std::string Sexy::Lower(const std::string& _data)
{
	std::string s = _data;
	std::transform(s.begin(), s.end(), s.begin(), tolower);
	return s;
}

std::wstring Sexy::Lower(const std::wstring& _data)
{
	std::wstring s = _data;
	std::transform(s.begin(), s.end(), s.begin(), towlower);
	return s;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
int Sexy::StrFindNoCase(const char *theStr, const char *theFind)
{
	int p1,p2;
	int cp = 0;
	const int len1 = (int)strlen(theStr);
	const int len2 = (int)strlen(theFind);
	while(cp < len1)
	{
		p1 = cp;
		p2 = 0;
		while(p1<len1 && p2<len2)
		{
			if(tolower(theStr[p1])!=tolower(theFind[p2]))
				break;

			p1++; p2++;
		}
		if(p2==len2)
			return p1-len2;

		cp++;
	}

	return -1;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
bool Sexy::StrPrefixNoCase(const char *theStr, const char *thePrefix, int maxLength)
{
	int i;
	char c1 = 0, c2 = 0;
	for (i=0; i<maxLength; i++)
	{
		c1 = tolower(*theStr++);
		c2 = tolower(*thePrefix++);

		if (c1==0 || c2==0)
			break;

		if (c1!=c2)
			return false;
	}

	return c2==0 || i==maxLength;
}

std::wstring Sexy::UTF8StringToWString(const std::string& theString)
{
#ifdef _WIN32
	// 直接写 std::wstring，去掉 new wchar_t[] 中转；转换含终止符，按旧语义
	// 截断到首个 L'\0'（普通输入长度不变，仍等于旧 buffer 构造结果）。
	const int aSize = MultiByteToWideChar(CP_UTF8, 0, theString.c_str(), static_cast<int>(theString.length()) + 1, nullptr, 0);
	std::wstring aResult(aSize, L'\0');
	MultiByteToWideChar(CP_UTF8, 0, theString.c_str(), static_cast<int>(theString.length()) + 1, aResult.data(), aSize);
	aResult.resize(wcslen(aResult.c_str()));
	return aResult;
#elif defined(__ANDROID__)
	// NDK libc++ 已移除 <codecvt>；手写 UTF-8 → UTF-32(wchar_t=4B on android)
	std::wstring aResult;
	aResult.reserve(theString.size());
	for (size_t i = 0; i < theString.size();)
	{
		const unsigned char c = static_cast<unsigned char>(theString[i]);
		unsigned codepoint = 0; int extra = 0;
		if (c < 0x80) { codepoint = c; }
		else if ((c & 0xE0) == 0xC0) { codepoint = c & 0x1Fu; extra = 1; }
		else if ((c & 0xF0) == 0xE0) { codepoint = c & 0x0Fu; extra = 2; }
		else { codepoint = c & 0x07u; extra = 3; }
		i += 1;
		for (int k = 0; k < extra && i < theString.size(); ++k, ++i)
			codepoint = (codepoint << 6) | (static_cast<unsigned char>(theString[i]) & 0x3Fu);
		aResult.push_back(static_cast<wchar_t>(codepoint));
	}
	return aResult;
#else
	std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
	return converter.from_bytes(theString);
#endif
}

void Sexy::SMemR(void*& _Src, void* _Dst, size_t _Size)
{
	memcpy(_Dst, _Src, _Size);
	_Src = (void*)((size_t)_Src + _Size);
}

void Sexy::SMemRStr(void*& _Src, std::string& theString)
{
	size_t aStrLen;
	SMemR(_Src, &aStrLen, sizeof(aStrLen));
	theString.resize(aStrLen);
	SMemR(_Src, (void*)theString.c_str(), aStrLen);
}

void Sexy::SMemW(void*& _Dst, const void* _Src, size_t _Size)
{
	memcpy(_Dst, _Src, _Size);
	_Dst = (void*)((uintptr_t)_Dst + _Size);
}

void Sexy::SMemWStr(void*& _Dst, const std::string& theString)
{
	size_t aStrLen = theString.size();
	SMemW(_Dst, &aStrLen, sizeof(aStrLen));
	SMemW(_Dst, theString.c_str(), aStrLen);
}

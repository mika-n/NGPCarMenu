//
// D3D9Helpers. Various helper functions for D3D9 interfaces and string/wstring helper funcs.
//
//
// Copyright 2020, MIKA-N. https://github.com/mika-n
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of this softwareand associated
// documentation files(the "Software"), to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense, and to permit persons to whom the Software
// is furnished to do so, subject to the following conditions :
// 
// - The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
// - The derived work or derived parts are also "open sourced" and the source code or part of the work using components
//   from this project is made publicly available with modifications to this base work.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE AUTHORS
// OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//

#include "stdafx.h"

//#include <memory>			// make_unique
#include <string>
#include <algorithm>		// transform
#include <vector>			// vector
#include <sstream>			// wstringstream
#include <filesystem>		// fs::exists
#include <fstream>			// std::ifstream and ofstream

#include <locale>			// UTF8 locales
#include <codecvt>

#include <wincodec.h>		// IWICxx image funcs
#include <shlwapi.h>		// PathRemoveFileSpec
#include <shellapi.h>		// CommandLineToArgvW

#include <winver.h>			// GetFileVersionInfo
#pragma comment(lib, "version.lib")

#include <gdiplus.h>
#include <gdiplusimaging.h>
#pragma comment(lib,"gdiplus.lib")

#include <d3d9.h>
#pragma comment(lib, "d3d9.lib") 

#include "D3D9Helpers.h"

#ifndef D3DX_PI
//#define D3DX_PI 3.1415926535897932384626
#define D3DX_PI 3.14159265f
#endif

#define C_LOGMESSAGEBUFFER_SIZE 2048
BYTE g_LogMessageBuffer[C_LOGMESSAGEBUFFER_SIZE+2] = {0}; // Buffer for LogPrint and DebugPrint (char and wchar) message line (used within a log critical section, so only one msg at any time)

namespace fs = std::filesystem;

//
// Case-insensitive str comparison functions (starts with, ends with). 
//   s1 and s2 - Strings to compare (case-insensitive)
//   s2AlreadyInLowercase - If true then s2 is already in lowercase letters, so "optimize" the comparison by converting only s1 to lowercase
//   Return TRUE=Equal, FALSE=NotEqual
//
inline bool _iStarts_With(std::wstring s1, std::wstring s2, bool s2AlreadyInLowercase)
{
	transform(s1.begin(), s1.end(), s1.begin(), ::tolower);
	if (!s2AlreadyInLowercase) transform(s2.begin(), s2.end(), s2.begin(), ::tolower);
	return s1._Starts_with(s2);
}

inline bool _iStarts_With(std::string s1, std::string s2, bool s2AlreadyInLowercase)
{
	transform(s1.begin(), s1.end(), s1.begin(), ::tolower);
	if (!s2AlreadyInLowercase) transform(s2.begin(), s2.end(), s2.begin(), ::tolower);
	return s1._Starts_with(s2);
}

inline bool _iEnds_With(std::wstring s1, std::wstring s2, bool s2AlreadyInLowercase)
{
	transform(s1.begin(), s1.end(), s1.begin(), ::tolower);
	if (!s2AlreadyInLowercase) transform(s2.begin(), s2.end(), s2.begin(), ::tolower);
	if (s1.length() >= s2.length())
		return (s1.compare(s1.length() - s2.length(), s2.length(), s2) == 0);
	else
		return false;
}

inline bool _iEnds_With(std::string s1, std::string s2, bool s2AlreadyInLowercase)
{
	transform(s1.begin(), s1.end(), s1.begin(), ::tolower);
	if (!s2AlreadyInLowercase) transform(s2.begin(), s2.end(), s2.begin(), ::tolower);
	if (s1.length() >= s2.length())
		return (s1.compare(s1.length() - s2.length(), s2.length(), s2) == 0);
	else
		return false;
}

inline bool _iEqual_wchar(wchar_t c1, wchar_t c2)
{
	return std::tolower(c1) == std::tolower(c2);
}

inline bool _iEqualS2Lower_wchar(wchar_t c1, wchar_t c2)
{
	return std::tolower(c1) == c2;
}

inline bool _iEqual_char(char c1, char c2)
{
	return std::tolower(c1) == std::tolower(c2);
}

inline bool _iEqualS2Lower_char(char c1, char c2)
{
	return std::tolower(c1) == c2;
}

inline bool _iEqual(const std::wstring& s1, const std::wstring& s2, bool s2AlreadyInLowercase)
{
	if (s2AlreadyInLowercase)
		return (s1.size() == s2.size() && std::equal(s1.begin(), s1.end(), s2.begin(), _iEqualS2Lower_wchar));
	else
		return (s1.size() == s2.size() && std::equal(s1.begin(), s1.end(), s2.begin(), _iEqual_wchar));
}

inline bool _iEqual(const std::string& s1, const std::string& s2, bool s2AlreadyInLowercase)
{
	if (s2AlreadyInLowercase)
		return (s1.size() == s2.size() && std::equal(s1.begin(), s1.end(), s2.begin(), _iEqualS2Lower_char));
	else
		return (s1.size() == s2.size() && std::equal(s1.begin(), s1.end(), s2.begin(), _iEqual_char));
}


size_t _iFind(std::wstring s1, std::wstring s2, bool s2AlreadyInLowercase)
{
	std::transform(s1.begin(), s1.end(), s1.begin(), ::tolower);
	if (!s2AlreadyInLowercase) std::transform(s2.begin(), s2.end(), s2.begin(), ::tolower);
	return s1.find(s1);
}

//
// Trim/LTrim/RTrim wstring objects (the string obj is trimmed in-place, the original string modified)
//
inline void _LTrim(std::wstring& s)
{
	s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](WCHAR c) { return !std::isspace(c); }));
}

inline void _LTrim(std::string& s)
{
	s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](char c) { return !std::isspace(c); }));
}

inline void _RTrim(std::wstring& s)
{
	s.erase(std::find_if(s.rbegin(), s.rend(), [](WCHAR c) { return !std::isspace(c); }).base(), s.end());
}

inline void _RTrim(std::string& s)
{
	s.erase(std::find_if(s.rbegin(), s.rend(), [](char c) { return !std::isspace(c); }).base(), s.end());
}

inline void _Trim(std::wstring& s)
{
	_LTrim(s);
	_RTrim(s);
}

inline void _Trim(std::string& s)
{
	_LTrim(s);
	_RTrim(s);
}

/*
inline std::wstring _LTrimCopy(std::wstring s)
{
	_LTrim(s);
	return s;
}

inline std::wstring _RTrimCopy(std::wstring s)
{
	_RTrim(s);
	return s;
}

inline std::wstring _TrimCopy(std::wstring s)
{
	_Trim(s);
	return s;
}
*/


//
// Search all occurences of keywords in a string and replace the keyword with another value. Optionally case-insenstive search
//
template<typename T>
T _ReplaceStr_T(const T& str, const T& searchKeyword, const T& replaceValue, bool caseInsensitive)
{
	T sResult;	
	T _str;
	T _searchKeyword = searchKeyword;
	T _replaceValue = replaceValue;
	const size_t searchKeywordLen = searchKeyword.length();

	if (caseInsensitive)
	{
		_ToLowerCase(_searchKeyword);
		_ToLowerCase(_replaceValue);
	}

	// If the searchKeyword is longer than the original string then there cannot be any search matches. Return the original string value.
	if (searchKeywordLen > str.length() || searchKeywordLen < 1 || _searchKeyword.compare(_replaceValue) == 0)
		return str;

	size_t pos;
	const size_t replaceSize = replaceValue.length();

	pos = 0;
	sResult = str;
	while(true)
	{
		// Locate the next searchKeyword substring (exit if no more matches)
		_str = sResult;
		if (caseInsensitive)
			_ToLowerCase(_str);

		if (pos >= _str.length())
			break;

		pos = _str.find(_searchKeyword, pos);
		if (pos == T::npos)
			break;

		if (searchKeywordLen == replaceSize)
			// If the search keyword and replace value have the same length then use replace method to replace the value without copying the whole string
			sResult.replace(pos, searchKeywordLen, replaceValue);
		else
		{
			sResult.erase(pos, searchKeywordLen);
			sResult.insert(pos, replaceValue);
		}

		pos += replaceValue.length();
	}

	return sResult;
}

inline std::string _ReplaceStr(const std::string& str, const std::string& searchKeyword, const std::string& replaceValue, bool caseInsensitive)
{
	return _ReplaceStr_T<std::string>(str, searchKeyword, replaceValue, caseInsensitive);
}

inline std::wstring _ReplaceStr(const std::wstring& str, const std::wstring& searchKeyword, const std::wstring& replaceValue, bool caseInsensitive)
{
	return _ReplaceStr_T<std::wstring>(str, searchKeyword, replaceValue, caseInsensitive);
}

template<typename T, typename TC>
T _RemoveEnclosingChar_T(const T& str, const TC searchChar, bool caseInsensitive)
{
	T sResult = str;

	// Remove leading "enclosing" char
	if (sResult.length() >= 1 && ( (caseInsensitive && ::tolower(sResult[0]) == ::tolower(searchChar)) || (!caseInsensitive && sResult[0] == searchChar) ))
	{
		sResult.erase(0, 1);
		_Trim(sResult);
	}

	// Remove trailing "enclosing" char
	if (sResult.length() >= 1 && ((caseInsensitive && ::tolower(sResult[sResult.length() - 1]) == ::tolower(searchChar)) || (!caseInsensitive && sResult[sResult.length() - 1] == searchChar) ))
	{
		sResult.erase(sResult.length()-1, 1);
		_Trim(sResult);
	}

	return sResult;
}

inline std::wstring _RemoveEnclosingChar(const std::wstring& str, const WCHAR searchChar, bool caseInsensitive)
{
	return _RemoveEnclosingChar_T<std::wstring, WCHAR>(str, searchChar, caseInsensitive);
}

inline std::string _RemoveEnclosingChar(const std::string& str, const char searchChar, bool caseInsensitive)
{
	return _RemoveEnclosingChar_T<std::string, char>(str, searchChar, caseInsensitive);
}


//
// Convert ASCII char string to WCHAR string object or vice-versa
//
inline std::wstring _ToWString(const std::string& s)
{
	auto buf = std::make_unique<WCHAR[]>(s.length() + 1);
	mbstowcs_s(nullptr, buf.get(), s.length() + 1, s.c_str(), s.length());
	return std::wstring{ buf.get() };
}


inline std::string _ToString(const std::wstring& s)
{
	auto buf = std::make_unique<char[]>(s.length() + 1);
	wcstombs_s(nullptr, buf.get(), s.length()+1, s.c_str(), s.length());
	return std::string{ buf.get() };
}

inline std::string _ToUTF8String(const wchar_t* wszTextBuf, int iLen)
{
	std::string sResult;
	int iChars = ::WideCharToMultiByte(CP_UTF8, 0, wszTextBuf, iLen, nullptr, 0, nullptr, nullptr);
	if (iChars > 0)
	{
		sResult.resize(iChars);
		::WideCharToMultiByte(CP_UTF8, 0, wszTextBuf, iLen, const_cast<char*>(sResult.c_str()), iChars, nullptr, nullptr);
	}
	return sResult;
}

inline std::string _ToUTF8String(const std::wstring& s)
{
	return ::_ToUTF8String(s.c_str(), (int)s.size());
}

inline std::wstring _ToUTF8WString(const char* szTextBuf, int iLen)
{
	std::wstring sResult;
	int iChars = ::MultiByteToWideChar(CP_UTF8, 0, szTextBuf, -1, nullptr, 0);
	if (iChars > 0)
	{
		sResult.resize(iChars+1);
		::MultiByteToWideChar(CP_UTF8, 0, szTextBuf, -1, const_cast<WCHAR*>(sResult.c_str()), iChars);
	}
	return sResult;
}

inline std::wstring _ToUTF8WString(const std::string& s)
{
	return ::_ToUTF8WString(s.c_str(), (int)s.size());
}

inline std::wstring _ToUTF8WString(const std::wstring& s)
{
	//return ::_ToWString(::_ToUTF8String(s.c_str(), (int)s.size()));
	return ::_ToWString(::_ToUTF8String(s));
}


// Decode UTF8 encoded string back to "normal" (fex R\xe4m\xf6 -> Rämö)
std::wstring _DecodeUtf8String(const std::wstring& s_encoded)
{
	std::wstring sResult;
	sResult.reserve(64);

	int iLen = s_encoded.length();
	int iUTF8Value;

	for (int i = 0; i < iLen; i++)
	{

		if (i + 4 <= iLen && s_encoded[i] == L'\\' && s_encoded[i + 1] == L'x'
			&& ((s_encoded[i + 2] >= L'0' && s_encoded[i + 2] <= L'9')
				|| (s_encoded[i + 2] >= L'a' && s_encoded[i + 2] <= L'f')
				|| (s_encoded[i + 2] >= L'A' && s_encoded[i + 2] <= L'F'))
			&& ((s_encoded[i + 3] >= L'0' && s_encoded[i + 3] <= L'9')
				|| (s_encoded[i + 3] >= L'a' && s_encoded[i + 3] <= L'f')
				|| (s_encoded[i + 3] >= L'A' && s_encoded[i + 3] <= L'F'))
			)
		{
			WCHAR input;

			iUTF8Value = 0;
			input = s_encoded[i + 2];
			if (input >= L'0' && input <= L'9') iUTF8Value = input - L'0';
			if (input >= L'A' && input <= L'F') iUTF8Value = input - L'A' + 10;
			if (input >= L'a' && input <= L'f') iUTF8Value = input - L'a' + 10;
			iUTF8Value = iUTF8Value << 4;

			input = s_encoded[i + 3];
			if (input >= L'0' && input <= L'9') iUTF8Value += input - L'0';
			if (input >= L'A' && input <= L'F') iUTF8Value += input - L'A' + 10;
			if (input >= L'a' && input <= L'f') iUTF8Value += input - L'a' + 10;

			sResult += WCHAR(iUTF8Value);
			i += 3;
		}
		else
			sResult += s_encoded[i];
	}

	return sResult;
}


inline void _ToLowerCase(std::string& s)
{
	transform(s.begin(), s.end(), s.begin(), ::tolower);
}

inline void _ToLowerCase(std::wstring& s)
{
	transform(s.begin(), s.end(), s.begin(), ::tolower);
}

// Split string borrowed from the excellent PyString (Python string functions for std C++) string helper routine pack. Slightly modified version here.
// PyString copyright notice: Copyright (c) 2008-2010, Sony Pictures Imageworks Inc. All rights reserved. The source code is licensed to public domain by SonyPictures.
// https://github.com/imageworks/pystring/blob/master/pystring.cpp
int _SplitString(const std::string& s, std::vector<std::string>& splittedTokens, std::string sep, bool caseInsensitiveSep, bool sepAlreadyLowercase, int maxTokens)
{
	splittedTokens.clear();
	if (s.empty())
		return 0;

	if (sep.size() == 0)
	{
		splittedTokens.push_back(s);
		return 1;
	}

	if (caseInsensitiveSep && !sepAlreadyLowercase)
		_ToLowerCase(sep);

	std::string::size_type i, j, len = s.size(), n = sep.size();
	std::string s_substr;
	char s_char;

	i = j = 0;
	while (i + n <= len)
	{
		s_char = (caseInsensitiveSep ? tolower(s[i]) : s[i]);
		if (s_char == sep[0])
		{
			s_substr = s.substr(i, n);
			if (caseInsensitiveSep) 
				_ToLowerCase(s_substr);

			if (s_substr == sep)
			{
				if (maxTokens-- <= 0)
					break;

				splittedTokens.push_back(s.substr(j, i - j));
				i = j = i + n;
			}
		}
		else
		{
			i++;
		}
	}

	splittedTokens.push_back(s.substr(j, len - j));

	return splittedTokens.size();
}

inline int _SplitInHalf(const std::string& s, std::vector<std::string>& splittedTokens, std::string sep, bool caseInsensitiveSep, bool sepAlreadyLowercase)
{
	return _SplitString(s, splittedTokens, sep, caseInsensitiveSep, sepAlreadyLowercase, 2);
}


bool _IsAllDigit(const std::string& s)
{
	if (s.length() <= 0) return false;

	for (size_t i = 0; i < s.length(); i++)
		if (!isdigit(s[i])) return false;

	return true;
}

bool _IsAllDigit(const std::wstring& s)
{
	if (s.length() <= 0) return false;

	for (size_t i = 0; i < s.length(); i++)
		if (!iswdigit(s[i])) return false;

	return true;
}


// Convert BYTE integer to bit field string (fex. byte value 0x05 is printed as "00000101"
std::string _ToBinaryBitString(BYTE byteValue)
{
	std::string sResult;
	sResult.reserve(8);
	for (int bitIdx = 0; bitIdx < 8; bitIdx++)
	{
		sResult += ((byteValue & 0x80) ? "1" : "0"); // Start from the highest bit because otherwise the bit string field would be in reverse order
		byteValue <<= 1;
	}
	return sResult;
}


//
// Split "12 34 54 45" string and populate RECT struct with splitted values
//
bool _StringToRect(const std::wstring& s, RECT* outRect, const wchar_t separatorChar)
{
	std::vector<std::wstring> items;

	if (outRect == nullptr) return false;

	try
	{
		std::wstring item;
		std::wstringstream wss(s);
		while (std::getline(wss, item, separatorChar))
		{
			_Trim(item);
			if (!item.empty())
				items.push_back(item);
		}

		if (items.size() >= 1) outRect->left = std::stoi(items[0]);
		else outRect->left = 0;

		if (items.size() >= 2) outRect->top = std::stoi(items[1]);
		else outRect->top = 0;

		if (items.size() >= 3) outRect->right = std::stoi(items[2]);
		else outRect->right = 0;

		if (items.size() >= 4) outRect->bottom = std::stoi(items[3]);
		else outRect->bottom = 0;
	}
	catch (...)
	{
		// String to int conversion failed. Check input string value.
		return false;
	}

	return (items.size() >= 4);
}

bool _StringToRect(const std::string& s, RECT* outRect, const char separatorChar)
{
	std::vector<std::string> items;

	if (outRect == nullptr) return false;

	try
	{
		std::string item;
		std::stringstream wss(s);
		while (std::getline(wss, item, separatorChar))
		{
			_Trim(item);
			if (!item.empty())
				items.push_back(item);
		}

		if (items.size() >= 1) outRect->left = std::stoi(items[0]);
		else outRect->left = 0;

		if (items.size() >= 2) outRect->top = std::stoi(items[1]);
		else outRect->top = 0;

		if (items.size() >= 3) outRect->right = std::stoi(items[2]);
		else outRect->right = 0;

		if (items.size() >= 4) outRect->bottom = std::stoi(items[3]);
		else outRect->bottom = 0;
	}
	catch (...)
	{
		// String to int conversion failed. Check input string value.
		return false;
	}

	return (items.size() >= 4);
}

//
// Split "12 14" string as POINT x y values
//
bool _StringToPoint(const std::wstring& s, POINT* outPoint, const wchar_t separatorChar, long defaultValue)
{
	std::vector<std::wstring> items;

	if (outPoint == nullptr) return false;

	try
	{
		std::wstring item;
		std::wstringstream wss(s);
		while (std::getline(wss, item, separatorChar))
		{
			_Trim(item);
			if (!item.empty())
				items.push_back(item);
		}

		if (items.size() >= 1) outPoint->x= std::stoi(items[0]);
		else outPoint->x = defaultValue;

		if (items.size() >= 2) outPoint->y = std::stoi(items[1]);
		else outPoint->y = defaultValue;
	}
	catch (...)
	{
		// String to int conversion failed. Check input string value.
		return false;
	}

	return (items.size() >= 2);
}

bool _StringToPoint(const std::string& s, POINT* outPoint, const char separatorChar, long defaultValue)
{
	std::vector<std::string> items;

	if (outPoint == nullptr) return false;

	try
	{
		std::string item;
		std::stringstream wss(s);
		while (std::getline(wss, item, separatorChar))
		{
			_Trim(item);
			if (!item.empty())
				items.push_back(item);
		}

		if (items.size() >= 1) outPoint->x = std::stoi(items[0]);
		else outPoint->x = defaultValue;

		if (items.size() >= 2) outPoint->y = std::stoi(items[1]);
		else outPoint->y = defaultValue;
	}
	catch (...)
	{
		// String to int conversion failed. Check input string value.
		return false;
	}

	return (items.size() >= 2);
}


// Return the value of specified command line argument (fex "RichardBurnsRally_SSE.exe -AutoLogonParam1 myRun.rpl" would have -AutoLogonParam1 arg)
std::wstring GetCmdLineArgValue(const std::wstring& argName)
{
	std::wstring sResult;
	LPWSTR* wszArglist;
	int     nArgs;

	wszArglist = ::CommandLineToArgvW(::GetCommandLineW(), &nArgs);
	for (int idx = 0; idx < nArgs; idx++)
	{
		if (_iEqual(wszArglist[idx], argName))
		{
			idx++;
			sResult = (idx < nArgs ? wszArglist[idx] : L"");
			break;
		}
	}

	_Trim(sResult);
	sResult = _RemoveEnclosingChar(sResult, L'"', false);
	return sResult;
}

std::string GetCmdLineArgValue(const std::string& argName)
{
	return _ToString(::GetCmdLineArgValue(_ToWString(argName)));
}


// Is the file in UTF16 format instead of UTF8 or ANSI-ASCII?
bool _IsFileInUTF16Format(const std::string& fileName)
{
	BYTE buffer[2] = { 0 };

	std::ifstream srcFile(fileName, std::ifstream::binary | std::ios::in);
	if (!srcFile) return FALSE;	
	srcFile.read((char*)&buffer, sizeof(buffer));

	if ( (buffer[0] == 0xFF && buffer[1] == 0xFE)   // UTF16 little-endian LE
	  || (buffer[0] == 0xFE && buffer[1] == 0xFF)	// UTF16 big-endian BE
	)
		return TRUE;

	return FALSE;
}

bool _IsFileInUTF16Format(const std::wstring& fileName)
{
	return _IsFileInUTF16Format(_ToString(fileName));
}

int _FindMatchingFileNames(const std::string& fileSearchPath, std::vector<std::string>& resultList, bool resultWithFullPath, int maxResults)
{
	WIN32_FIND_DATA fd {0};
	HANDLE hFindFile = ::FindFirstFile(fileSearchPath.c_str(), &fd);
	int iFoundFiles = 0;

	if (hFindFile == INVALID_HANDLE_VALUE)
		return 0;

	std::string sPath;
	if (resultWithFullPath) sPath = fs::path(fileSearchPath).parent_path().string();

	resultList.reserve(5);
	while (TRUE)
	{
		if (fd.cFileName[0] != '\0')
		{
			resultList.push_back((resultWithFullPath ? sPath + "\\" + fd.cFileName : fd.cFileName));
			iFoundFiles++;
		}

		if (iFoundFiles <= maxResults || ::FindNextFile(hFindFile, &fd) == FALSE)
			break;
	}
	return iFoundFiles;
}

int _FindMatchingFileNames(const std::wstring& fileSearchPath, std::vector<std::wstring>& resultList, bool resultWithFullPath, int maxResults)
{
	WIN32_FIND_DATAW fd{ 0 };
	HANDLE hFindFile = ::FindFirstFileW(fileSearchPath.c_str(), &fd);
	int iFoundFiles = 0;

	if (hFindFile == INVALID_HANDLE_VALUE)
		return 0;

	std::wstring sPath;
	if(resultWithFullPath) sPath = fs::path(fileSearchPath).parent_path().wstring();

	resultList.reserve(5);
	while (TRUE)
	{
		if (fd.cFileName[0] != L'\0')
		{
			resultList.push_back((resultWithFullPath ? sPath + L"\\" + fd.cFileName : fd.cFileName));
			iFoundFiles++;
		}

		if (iFoundFiles <= maxResults || ::FindNextFileW(hFindFile, &fd) == FALSE)
			break;
	}
	return iFoundFiles;
}


// Read UTF16 file content and convert it to multibyte UTF8 string
std::string _ConvertUTF16FileContentToUTF8(const std::string& fileName)
{
	std::string sResult;
	size_t fileSize;
	std::ifstream srcFile(fileName, std::ios::binary);

	srcFile.seekg(0, std::ios::end);
	fileSize = (size_t)srcFile.tellg();
	if (fileSize >= 4)
	{
		// Skip UTF16 BOM header bytes
		srcFile.seekg(2, std::ios::beg);
		fileSize -= 2;

		// Convert UTF16 wchar to UTF8 multibyte string
		std::u16string u16Str((fileSize / 2) + 1, '\0');
		srcFile.read((char*)&u16Str[0], fileSize);
		sResult = _ToUTF8String((wchar_t*)&u16Str[0]);
	}

	return sResult;
}


// Return the version tag of a file as string, for example "1.0.4.0"
std::string GetFileVersionInformationAsString(const std::wstring& fileName)
{
	std::string sResult;
	UINT iMajor, iMinor, iPatch, iBuild;

	if (GetFileVersionInformationAsNumber(fileName, &iMajor, &iMinor, &iPatch, &iBuild))
		sResult = std::to_string(iMajor) + "." + std::to_string(iMinor) + "." + std::to_string(iPatch) + "." + std::to_string(iBuild);

	// Empty string if getVer failed
	return sResult;
}

BOOL GetFileVersionInformationAsNumber(const std::wstring& fileName, UINT* pMajorVer, UINT* pMinorVer, UINT* pPatchVer, UINT* pBuildVer)
{
	BOOL  bResult = FALSE;
	DWORD dwHandle = 0;
	DWORD dwSize = GetFileVersionInfoSizeW(fileName.c_str(), &dwHandle);

	if (dwSize > 0)
	{
		LPBYTE pVerData = new BYTE[dwSize];
		if (GetFileVersionInfoW(fileName.c_str(), 0 /*dwHandle*/, dwSize, pVerData))
		{
			LPBYTE pQryBuffer = nullptr;
			UINT iLen = 0;

			if (VerQueryValue(pVerData, "\\", (LPVOID*)&pQryBuffer, &iLen))
			{
				if (iLen > 0 && pQryBuffer != nullptr)
				{
					VS_FIXEDFILEINFO* verInfo = (VS_FIXEDFILEINFO*)pQryBuffer;
					if (verInfo->dwSignature == 0xfeef04bd)
					{
						if (pMajorVer != nullptr) *pMajorVer = (UINT)((verInfo->dwFileVersionMS >> 16) & 0xffff);
						if (pMinorVer != nullptr) *pMinorVer = (UINT)((verInfo->dwFileVersionMS >> 0) & 0xffff);
						if (pPatchVer != nullptr) *pPatchVer = (UINT)((verInfo->dwFileVersionLS >> 16) & 0xffff);
						if (pBuildVer != nullptr) *pBuildVer = (UINT)((verInfo->dwFileVersionLS >> 0) & 0xffff);
						bResult = TRUE;
					}
				}
			}
		}
		delete[] pVerData;
	}

	return bResult;
}


std::string GetGUIDAsString()
{
	std::string sResult;
	GUID guid;

	if (CoCreateGuid(&guid) == S_OK)
	{
		char szBuffer[40];
		snprintf(szBuffer, sizeof(szBuffer), "%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x", guid.Data1, guid.Data2, guid.Data3, guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3], guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);
		sResult = szBuffer;
	}

	return sResult;
}


void GetCurrentDateAndTimeAsYYYYMMDD_HHMISS(int* pCurrentDate, std::string* pCurrentTime)
{
	std::time_t rawtime;
	std::tm* timeinfo;
	char buffer[80]{};

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable:4996)
#endif
	std::time(&rawtime);
	timeinfo = std::localtime(&rawtime);
#if defined(_MSC_VER) 
#pragma warning(pop)
#endif

	std::strftime(buffer, 80, "%Y%m%d %H%M%S", timeinfo);
	//std::puts(buffer);
	buffer[8] = '\0';

	if (pCurrentDate) *pCurrentDate = atoi(buffer);
	if (pCurrentTime) *pCurrentTime = &buffer[9];
}

// Return value in seconds as MI:SS,MS formatted string value (fex 145.23 secs -> 02:25,2)
std::string GetSecondsAsMISSMS(float valueSecs, bool padWithTwoDigits, bool prefixPlusSign) 
{
	std::string signChar;
	if (valueSecs < 0)
	{
		signChar = "-";
		valueSecs = abs(valueSecs);
	}
	else if (prefixPlusSign)
		signChar = "+";

	double valueInSecs = RoundFloatToDouble(valueSecs, 1);

	// Minutes with padded to minimum of two chars
	std::string min = std::to_string(static_cast<int>(floor(valueInSecs / 60.0)));
	if(padWithTwoDigits && min.length() < 2) min = std::string(2 - min.length(), '0') + min;

	double secWithDecimals = fmod(valueInSecs, 60.0);
	double secPart, msecPart;
	msecPart = std::modf(secWithDecimals, &secPart);

	//DebugPrint("valueInSecs=%lf  secWithDecimals=%lf  secPart=%lf  msecPart=%lf", valueInSecs, secWithDecimals, secPart, msecPart);

	// Seconds (with one decimal precision)
	std::string sec = std::to_string(static_cast<int>(secPart));
	if (padWithTwoDigits && sec.length() < 2) sec = std::string(2 - sec.length(), '0') + sec;
	sec += std::to_string(msecPart).substr(1,2);

	if (min == "00" || min == "0") return signChar + sec;
	else return signChar + min + ":" + sec;
}

// Return valueInSecs+lengthInKm as km/h string
std::string GetSecondsAsKMh(float valueInSecs, float lengthInMeters, bool postfixKmh, int outputPrecision)
{
	std::string sResult;
	double kmh;

	if (valueInSecs == 0.0f || lengthInMeters == 0.0f)
		return sResult;

	//kmh = static_cast<int>(floorf( ((lengthInMeters / 1000.0f) / valueInSecs) * 3600.0f ));
	//sResult = std::to_string(kmh) + (postfixKmh ? " km/h" : "");
	//return sResult;

	kmh = RoundFloatToDouble(((lengthInMeters / 1000.0f) / valueInSecs) * 3600.0f, 1);
	double deciPart, intPart;
	deciPart = std::modf(kmh, &intPart);

	sResult = std::to_string(static_cast<int>(intPart));
	if (outputPrecision > 0) sResult += std::to_string(deciPart).substr(1, outputPrecision + 1);
	return sResult + (postfixKmh ? " km/h" : "");
}


// Round float to nearest X decimals
double RoundFloatToDouble(float value, int decimals)
{
	//if (decimals == 0) return round(value);
	//else return static_cast<double>(roundf(value * powf(10, decimals))) / 1000.0;
	if (decimals == 0) return static_cast<double>(roundf(value));
	double d0 = pow(10, decimals);
	double d1 = d0 * 10;
	return (static_cast<int>((value + (5.0 / d1)) * d0)) / d0;
}

// Round float down to nearest X decimals
double FloorFloatToDouble(float value, int decimals)
{
	if (decimals == 0) return static_cast<double>(floorf(value));
	double d0 = pow(10, decimals);
	return (static_cast<int>(value * d0)) / d0;
}

// Round float down to nearest X decimals
float FloorFloat(float value, int decimals)
{
	double dValue = FloorFloatToDouble(value, decimals);
	return static_cast<float>(dValue);
}

//---------------------------------------------------------------------------------------------------------------

//-----------------------------------------------------------------------------------------------------------------------------
// Debug printout functions. Not used in release build.
//

unsigned long g_iLogMsgCount = 0;  // Safety precaution in debug logger to avoid flooding the logfile. One process running prints out only max N debug lines (more than that then the plugin is probably in some infinite loop lock)
std::wstring g_sLogFileName;
std::ofstream* g_fpLogFile = nullptr;
CRITICAL_SECTION g_hLogCriticalSection;

void DebugOpenFile(bool bOverwriteFile = false)
{
	try
	{
		if (g_fpLogFile == nullptr)
		{
			if (g_sLogFileName.empty())
			{
				wchar_t  szModulePath[_MAX_PATH];
				::GetModuleFileNameW(NULL, szModulePath, COUNT_OF_ITEMS(szModulePath));
				::PathRemoveFileSpecW(szModulePath);

				g_sLogFileName = szModulePath;
				g_sLogFileName = g_sLogFileName + L"\\Plugins\\" L"" VS_PROJECT_NAME L"\\" L"" VS_PROJECT_NAME L".log";

				// Disable VC+ warning about "return value ignored". We know it and this code really don't need the return value, so we ignore it on purpose
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable:6031)
#endif
				InitializeCriticalSectionAndSpinCount(&g_hLogCriticalSection, 0x00000400);
#if defined(_MSC_VER) 
#pragma warning(pop)
#endif

#ifndef USE_DEBUG
				// Release build creates always a new empty logfile when the logfile is opened for the first time during a process run
				bOverwriteFile = true;
#endif
			}

			// Overwrite or append the logfile
			//_wfopen_s(&g_fpLogFile, g_sLogFileName.c_str(), (bOverwriteFile ? L"w+t,ccs=UTF-8" : L"a+t,ccs=UTF-8"));
			g_fpLogFile = new std::ofstream(g_sLogFileName, (bOverwriteFile ? std::ios::out | std::ios::trunc : std::ios::out | std::ios::app) );
		}
	}
	catch (...)
	{
		// Do nothing
	}
}

void DebugReleaseResources()
{
	DeleteCriticalSection(&g_hLogCriticalSection);
}

void DebugCloseFile()
{
	std::ofstream* fpLogFile = g_fpLogFile;	
	g_fpLogFile = nullptr;
	if (fpLogFile != nullptr)
	{
		EnterCriticalSection(&g_hLogCriticalSection);
		fpLogFile->close();
		delete fpLogFile;
		LeaveCriticalSection(&g_hLogCriticalSection);
	}
}

// Print out CHAR or WCHAR log msg
void DebugPrintFunc_CHAR_or_WCHAR(LPCSTR szTxtBuf, LPCWSTR wszTxtBuf, int iMaxCharsInBuf)
{
	SYSTEMTIME t;
	char szTxtTimeStampBuf[32] = { 0 };
	bool bFirstMessage = g_iLogMsgCount == 0;

	try
	{
		g_iLogMsgCount++;

		GetLocalTime(&t);
		if (GetTimeFormat(LOCALE_USER_DEFAULT, 0, &t, "HH:mm:ss ", (LPSTR)szTxtTimeStampBuf, COUNT_OF_ITEMS(szTxtTimeStampBuf) - 1) <= 0)
			szTxtTimeStampBuf[0] = '\0';

		if (g_fpLogFile == nullptr)
			DebugOpenFile();

		if (g_fpLogFile)
		{
			if (bFirstMessage)
			{
				// Mark the output debug logfile as UTF8 file (just in case some debug msg contains utf8 chars)
				*g_fpLogFile << ::_ToUTF8String(std::wstring(L"" VS_PROJECT_NAME " "));
				*g_fpLogFile << (GetFileVersionInformationAsString(g_sLogFileName + L"\\..\\..\\" L"" VS_PROJECT_NAME L".dll")).c_str();
				*g_fpLogFile << "  https://github.com/mika-n/NGPCarMenu" << std::endl;
			}

			try
			{
				//EnterCriticalSection(&g_hLogCriticalSection);

				*g_fpLogFile << szTxtTimeStampBuf;
				if (szTxtBuf != nullptr)  *g_fpLogFile << szTxtBuf;
				if (wszTxtBuf != nullptr) *g_fpLogFile << ::_ToUTF8String(wszTxtBuf, iMaxCharsInBuf).c_str();
				*g_fpLogFile << std::endl;
				//DebugCloseFile();

				//LeaveCriticalSection(&g_hLogCriticalSection);
			}
			catch (...)
			{
				//LeaveCriticalSection(&g_hLogCriticalSection);
			}
		}
	}
	catch (...)
	{
		// Do nothing
	}
}

void DebugPrintFunc(LPCSTR lpszFormat, ...)
{
	// Safety option to ignore debug messages if the app gets stuck in infinite loop
	if (g_iLogMsgCount >= 2000)
		return;

	va_list args;
	va_start(args, lpszFormat);

	try
	{
		EnterCriticalSection(&g_hLogCriticalSection);

		//CHAR szTxtBuf[2048];
		if (_vsnprintf_s((char*)g_LogMessageBuffer, C_LOGMESSAGEBUFFER_SIZE, _TRUNCATE, lpszFormat, args) <= 0)
			g_LogMessageBuffer[0] = 0;

		DebugPrintFunc_CHAR_or_WCHAR((char*)g_LogMessageBuffer, nullptr, C_LOGMESSAGEBUFFER_SIZE - 1);

		LeaveCriticalSection(&g_hLogCriticalSection);
	}
	catch (...)
	{
		LeaveCriticalSection(&g_hLogCriticalSection);
	}

	va_end(args);
}

void DebugPrintFunc(LPCWSTR lpszFormat, ...)
{
	// Safety option to ignore debug messages if the app gets stuck in infinite loop
	if (g_iLogMsgCount >= 2000)
		return;

	va_list args;
	va_start(args, lpszFormat);

	try
	{
		EnterCriticalSection(&g_hLogCriticalSection);

		//WCHAR wszTxtBuf[2048];
		if (_vsnwprintf_s((WCHAR*)g_LogMessageBuffer, C_LOGMESSAGEBUFFER_SIZE / sizeof(WCHAR), _TRUNCATE, lpszFormat, args) <= 0)
			((WCHAR*)g_LogMessageBuffer)[0] = L'\0';

		DebugPrintFunc_CHAR_or_WCHAR(nullptr, (WCHAR*)g_LogMessageBuffer, (C_LOGMESSAGEBUFFER_SIZE - 1) / sizeof(WCHAR));
		
		LeaveCriticalSection(&g_hLogCriticalSection);
	}
	catch (...)
	{
		LeaveCriticalSection(&g_hLogCriticalSection);
	}

	va_end(args);
}

void DebugClearFile()
{
	DebugCloseFile();
	DebugOpenFile(true);
}


#if USE_DEBUG == 1

void DebugDumpBuffer(byte* pBuffer, int iPreOffset = 0, int iBytesToDump = 64)
{
	char txtBuffer[64] = { 0 };

	byte* pBuffer2 = pBuffer - iPreOffset;
	for (int idx = 0; idx < iBytesToDump; idx += 8)
	{
		int iAppendPos = 0;
		for (int idx2 = 0; idx2 < 8; idx2++)
			iAppendPos += sprintf_s(txtBuffer + iAppendPos, sizeof(txtBuffer), "%02x ", (byte)pBuffer2[idx + idx2]);

		iAppendPos += sprintf_s(txtBuffer + iAppendPos, sizeof(txtBuffer), " | ");

		for (int idx2 = 0; idx2 < 8; idx2++)
			iAppendPos += sprintf_s(txtBuffer + iAppendPos, sizeof(txtBuffer), "%c", (pBuffer2[idx + idx2] < 32 || pBuffer2[idx + idx2] > 126) ? '.' : pBuffer2[idx + idx2]);

		DebugPrint(txtBuffer);
	}
}

#endif // USE_DEBUG



//---------------------------------------------------------------------------------------------------------------
HRESULT D3D9SavePixelsToFile32bppPBGRA(UINT width, UINT height, UINT stride, LPBYTE pixels, const std::wstring& filePath, const GUID& format, WICPixelFormatGUID pixelFormat);

// Return GDI+ encoder classid.
// GetEncoderClsid method borrowed from Windows documentation (https://docs.microsoft.com/en-us/windows/win32/gdiplus/-gdiplus-retrieving-the-class-identifier-for-an-encoder-use)
int GdiPlusGetEncoderClsid(const WCHAR* format, CLSID* pClsid)
{
	UINT  num = 0;          // number of image encoders
	UINT  size = 0;         // size of the image encoder array in bytes

	Gdiplus::ImageCodecInfo* pImageCodecInfo = NULL;
	Gdiplus::GetImageEncodersSize(&num, &size);
	if (size == 0)
		return -1;  // Failure

	pImageCodecInfo = (Gdiplus::ImageCodecInfo*)(malloc(size));
	if (pImageCodecInfo == NULL)
		return -1;  // Failure

	Gdiplus::GetImageEncoders(num, size, pImageCodecInfo);
	for (UINT j = 0; j < num; ++j)
	{
		// wcscmp warns about potentially dangerous function. Ignore the VC++ warning here because we know the following works and never does buffer overrun.
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable:6385)
#endif
		if (wcscmp(pImageCodecInfo[j].MimeType, format) == 0)
		{
			*pClsid = pImageCodecInfo[j].Clsid;
			free(pImageCodecInfo);
			return j;  // Success
		}
#if defined(_MSC_VER) 
#pragma warning(pop)
#endif
	}

	free(pImageCodecInfo);
	return -1;  // Failure
}


// Save screenshot file using GDI services instead of DirectX services
HRESULT D3D9SavePixelsToFileGDI(const HWND hAppWnd, RECT wndCaptureRect, const std::wstring& outputFileName, const GUID& format)
{
	HRESULT hResult;

	LPSTR pBuf = nullptr;
	HDC hdcAppWnd = nullptr;
	HDC hdcMemory = nullptr;
	HBITMAP hbmAppWnd = nullptr;
	HGLOBAL hDIB = nullptr; //  INVALID_HANDLE_VALUE;

	BITMAP hAppWndBitmap;
	BITMAPINFOHEADER bmpInfoHeader;
	BITMAPFILEHEADER bmpFileHeader = { 0 };

	Gdiplus::Bitmap* gdiBitmap = nullptr;

	hResult = S_OK;

	try
	{
		// Reading bitmap via DirectX frame buffers
		LogPrint(L"Using GDI image buffer to create %s", outputFileName.c_str());

		hdcAppWnd = GetDC(hAppWnd);
		if (hdcAppWnd) hdcMemory = CreateCompatibleDC(hdcAppWnd);

		if (!hdcMemory || !hdcAppWnd) hResult = E_INVALIDARG;

		if (SUCCEEDED(hResult) && wndCaptureRect.right == 0 && wndCaptureRect.bottom == 0 && wndCaptureRect.left == 0 && wndCaptureRect.right == 0)
		{
			// Capture rect all zeros. Take a screenshot of the whole D3D9 client window area (ie. RBR game content without WinOS wnd decorations)
			if (GetClientRect(hAppWnd, &wndCaptureRect) == false)
				hResult = E_INVALIDARG;
		}

		if (SUCCEEDED(hResult)) hbmAppWnd = CreateCompatibleBitmap(hdcAppWnd, wndCaptureRect.right - wndCaptureRect.left, wndCaptureRect.bottom - wndCaptureRect.top);
		if (!hbmAppWnd) hResult = E_INVALIDARG;

		if (SUCCEEDED(hResult))
		{
			HGDIOBJ hgdiResult = SelectObject(hdcMemory, hbmAppWnd);
			if (hgdiResult == nullptr || hgdiResult == HGDI_ERROR)
				hResult = E_INVALIDARG;
		}

		if (SUCCEEDED(hResult))
			if (!BitBlt(hdcMemory, 0, 0, wndCaptureRect.right - wndCaptureRect.left, wndCaptureRect.bottom - wndCaptureRect.top, hdcAppWnd, wndCaptureRect.left, wndCaptureRect.top, SRCCOPY | CAPTUREBLT))
				hResult = E_INVALIDARG;

		ZeroMemory(&hAppWndBitmap, sizeof(BITMAP));
		if (SUCCEEDED(hResult) && GetObject(hbmAppWnd, sizeof(BITMAP), &hAppWndBitmap) == 0)
			hResult = E_INVALIDARG;

		ZeroMemory(&bmpInfoHeader, sizeof(BITMAPINFOHEADER));
		bmpInfoHeader.biSize        = sizeof(BITMAPINFOHEADER);
		bmpInfoHeader.biWidth       = hAppWndBitmap.bmWidth;
		bmpInfoHeader.biHeight      = hAppWndBitmap.bmHeight;
		bmpInfoHeader.biPlanes      = 1;
		bmpInfoHeader.biBitCount    = hAppWndBitmap.bmBitsPixel; // 32;
		bmpInfoHeader.biCompression = BI_RGB;

		DWORD dwBmpSize = ((hAppWndBitmap.bmWidth * bmpInfoHeader.biBitCount + 31) / 32) * 4 * hAppWndBitmap.bmHeight;

		if (SUCCEEDED(hResult))	hDIB = GlobalAlloc(GHND, dwBmpSize);
		if (hDIB == nullptr) hResult = E_INVALIDARG;

		if (SUCCEEDED(hResult))	pBuf = (LPSTR)GlobalLock(hDIB);
		if (pBuf == nullptr) hResult = E_INVALIDARG;

		if (SUCCEEDED(hResult) && GetDIBits(hdcAppWnd, hbmAppWnd, 0, (UINT)hAppWndBitmap.bmHeight, pBuf, (BITMAPINFO*)&bmpInfoHeader, DIB_RGB_COLORS) == 0)
			hResult = E_INVALIDARG;

		if (SUCCEEDED(hResult))
			// Remove potentially existing output file
			::_wremove(outputFileName.c_str());

		// BMP or PNG output file format
		if (SUCCEEDED(hResult) && format == GUID_ContainerFormatBmp)
		{
			// Save BMP file
			std::ofstream outFile(outputFileName, std::ofstream::binary | std::ios::out);
			if (outFile)
			{
				bmpFileHeader.bfOffBits = (DWORD)sizeof(BITMAPFILEHEADER) + (DWORD)sizeof(BITMAPINFOHEADER);
				bmpFileHeader.bfSize = dwBmpSize + sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
				bmpFileHeader.bfType = 0x4D42; // BM file type tag in little endian mode

				outFile.write((LPCSTR)&bmpFileHeader, sizeof(BITMAPFILEHEADER));
				outFile.write((LPCSTR)&bmpInfoHeader, sizeof(BITMAPINFOHEADER));
				outFile.write(pBuf, dwBmpSize);
				outFile.flush();
				outFile.close();
			}
		}
		else if (SUCCEEDED(hResult))
		{
			// Save PNG file
			CLSID encoderClsid;
			Gdiplus::GdiplusStartupInput gdiplusStartupInput;
			ULONG_PTR gdiplusToken = 0;

			try
			{
				Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

				if (gdiplusToken != 0 && GdiPlusGetEncoderClsid(L"image/png", &encoderClsid) >= 0)
				{
					BITMAPINFO binfo;
					ZeroMemory(&binfo, sizeof(binfo));
					binfo.bmiHeader = bmpInfoHeader;
					binfo.bmiHeader.biSize = sizeof(binfo);
					binfo.bmiHeader.biSizeImage = dwBmpSize;

					gdiBitmap = Gdiplus::Bitmap::FromBITMAPINFO(&binfo, pBuf);
					if (gdiBitmap != nullptr)
						gdiBitmap->Save(outputFileName.c_str(), &encoderClsid);
				}
			}
			catch(...)
			{
				LogPrint("ERROR D3D9SavePixelsToFileGDI. %s GDI failed to create file", outputFileName.c_str());
			}

			SAFE_DELETE(gdiBitmap);
			if(gdiplusToken != 0) Gdiplus::GdiplusShutdown(gdiplusToken);
		}
	}
	catch (...)
	{
		hResult = E_INVALIDARG;
		LogPrint("ERROR D3D9SavePixelsToFileGDI. %s failed to create the file", outputFileName.c_str());
	}

	if (/*hDIB != INVALID_HANDLE_VALUE &&*/ hDIB != nullptr)
	{
		GlobalUnlock(hDIB);
		GlobalFree(hDIB);
	}

	if (hbmAppWnd) DeleteObject(hbmAppWnd);
	if (hdcMemory) DeleteObject(hdcMemory);
	if (hdcAppWnd) ReleaseDC(hAppWnd, hdcAppWnd);

	return hResult;
}


//
// Save bitmap buffer (taken from D3D9 surface) as PNG file
//
HRESULT D3D9SavePixelsToFile32bppPBGRA(UINT width, UINT height, UINT stride, LPBYTE pixels, const std::wstring& filePath, const GUID& format, WICPixelFormatGUID pixelFormat)
{
	if (!pixels || filePath.empty())
		return E_INVALIDARG;

	HRESULT hResult;
	HRESULT hResultCoInit = E_INVALIDARG;

	IWICImagingFactory* factory = nullptr;
	IWICBitmapEncoder* encoder = nullptr;
	IWICBitmapFrameEncode* frame = nullptr;
	IWICStream* stream = nullptr;
	//GUID pf = GUID_WICPixelFormat32bppPBGRA; // DX9 surface bitmap format
	//GUID pf = GUID_WICPixelFormat24bppBGR; ¨ // GDI bitmap format

	try
	{
		hResultCoInit = CoInitialize(nullptr);

		hResult = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));

		if (SUCCEEDED(hResult)) hResult = factory->CreateStream(&stream);
		
		// Remove potentially existing output file
		if (SUCCEEDED(hResult)) ::_wremove(filePath.c_str());
		
		if (SUCCEEDED(hResult)) hResult = stream->InitializeFromFilename(filePath.c_str(), GENERIC_WRITE);

		if (SUCCEEDED(hResult)) hResult = factory->CreateEncoder(format, nullptr, &encoder);
		if (SUCCEEDED(hResult)) hResult = encoder->Initialize(stream, WICBitmapEncoderNoCache);
		if (SUCCEEDED(hResult)) hResult = encoder->CreateNewFrame(&frame, nullptr);

		if (SUCCEEDED(hResult)) hResult = frame->Initialize(nullptr);
		if (SUCCEEDED(hResult)) hResult = frame->SetSize(width, height);
		if (SUCCEEDED(hResult)) hResult = frame->SetPixelFormat(&pixelFormat);
		if (SUCCEEDED(hResult)) hResult = frame->WritePixels(height, stride, stride * height, pixels);

		if (SUCCEEDED(hResult)) hResult = frame->Commit();
		if (SUCCEEDED(hResult)) hResult = encoder->Commit();
	}
	catch (...)
	{
		// Hmmmm.. Something went wrong...
		hResult = E_INVALIDARG;
	}

	SAFE_RELEASE(stream);
	SAFE_RELEASE(frame);
	SAFE_RELEASE(encoder);
	SAFE_RELEASE(factory);

	if (hResultCoInit == S_OK) CoUninitialize();
	
	return hResult;
}


//
// Take D3D9 screenshot of RBR screen (the whole game screen or specified capture rectangle only) and save it as PNG file.
// If the outputFileName extension defines the file format (.PNG or .BMP)
//
HRESULT D3D9SaveScreenToFile(const LPDIRECT3DDEVICE9 pD3Device, const HWND hAppWnd, RECT wndCaptureRect, const std::wstring& outputFileName)
{
	HRESULT hResult;

	IDirect3D9* d3d = nullptr;
	IDirect3DSurface9* surface = nullptr;
	LPBYTE screenshotBuffer = nullptr;
	D3DDISPLAYMODE mode;
	D3DLOCKED_RECT rc = { 0 };
	int pitch;

	GUID cf; // GUID_ContainerFormatPng or GUID_ContainerFormatBmp

	if (/*pD3Device == nullptr ||*/ outputFileName.empty())
		return E_INVALIDARG;

	try
	{
		cf = GUID_ContainerFormatPng;
		if (fs::path(outputFileName).extension() == ".bmp")
			cf = GUID_ContainerFormatBmp;
		
		// If DX9 device is NULL then save the output image file using GDI bitmap data instead of using DX9 framebuffer data
		if (pD3Device == nullptr)
			return D3D9SavePixelsToFileGDI(hAppWnd, wndCaptureRect, outputFileName, cf);

		// Reading bitmap via DirectX frame buffers
		LogPrint(L"Using DirectX image buffer to create %s", outputFileName.c_str());

		//d3d = Direct3DCreate9(D3D_SDK_VERSION);
		//d3d->GetAdapterDisplayMode(adapter, &mode)
		pD3Device->GetDirect3D(&d3d);
		hResult = d3d->GetAdapterDisplayMode(D3DADAPTER_DEFAULT, &mode);

		if (SUCCEEDED(hResult)) hResult = pD3Device->CreateOffscreenPlainSurface(mode.Width, mode.Height, D3DFMT_A8R8G8B8, D3DPOOL_SCRATCH /*D3DPOOL_SYSTEMMEM*/, &surface, nullptr);

		if (SUCCEEDED(hResult) && wndCaptureRect.right == 0 && wndCaptureRect.bottom == 0 && wndCaptureRect.left == 0 && wndCaptureRect.right == 0)
		{
			// Capture rect all zeros. Take a screenshot of the whole D3D9 client window area (ie. RBR game content without WinOS wnd decorations)
			if(!GetClientRect(hAppWnd, &wndCaptureRect))
			//	MapWindowPoints(hAppWnd, NULL, (LPPOINT)&wndCaptureRect, 2);
			//else
				hResult = E_INVALIDARG;
		}

		if (SUCCEEDED(hResult)) MapWindowPoints(hAppWnd, NULL, (LPPOINT)&wndCaptureRect, 2);

		// Compute the required bitmap buffer size and allocate it
		rc.Pitch = 4;
		if (SUCCEEDED(hResult)) hResult = surface->LockRect(&rc, &wndCaptureRect, /*0*/ D3DLOCK_NO_DIRTY_UPDATE | D3DLOCK_NOSYSLOCK | D3DLOCK_READONLY);
		
		pitch = rc.Pitch;
		if (SUCCEEDED(hResult)) hResult = surface->UnlockRect();

		if (SUCCEEDED(hResult)) screenshotBuffer = new BYTE[pitch * (wndCaptureRect.bottom - wndCaptureRect.top) /* img height */];
		if(screenshotBuffer == nullptr) hResult = E_INVALIDARG;

		if (SUCCEEDED(hResult)) hResult = pD3Device->GetFrontBufferData(0, surface);

		// DX fullscreen screenshot (Note! Doesn't work, so run RBR in DX9 window mode, RichardBurnsRally.ini fullsreen=false and use Fixup plugin to set fullscreen wnd mode)
		// g_pRBRIDirect3DDevice9->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &surface)

		// Copy frame to byte buffer
		if (SUCCEEDED(hResult)) hResult = surface->LockRect(&rc, &wndCaptureRect, D3DLOCK_NO_DIRTY_UPDATE | D3DLOCK_NOSYSLOCK | D3DLOCK_READONLY );
		if (SUCCEEDED(hResult)) CopyMemory(screenshotBuffer, rc.pBits, rc.Pitch * (wndCaptureRect.bottom - wndCaptureRect.top) /* img height */);
		if (SUCCEEDED(hResult)) hResult = surface->UnlockRect();

		if (SUCCEEDED(hResult)) hResult = D3D9SavePixelsToFile32bppPBGRA((wndCaptureRect.right - wndCaptureRect.left) /* img width */, (wndCaptureRect.bottom - wndCaptureRect.top) /* img height */, pitch, screenshotBuffer, outputFileName, cf, GUID_WICPixelFormat32bppPBGRA);

		//SavePixelsToFile32bppPBGRA(..., GUID_ContainerFormatDds)
		//SavePixelsToFile32bppPBGRA(..., GUID_ContainerFormatJpeg)
	}
	catch (...)
	{
		// Hmmmm.. Something went wrong...
		hResult = E_INVALIDARG;
	}

	if (screenshotBuffer) delete[] screenshotBuffer;

	SAFE_RELEASE(surface);
	SAFE_RELEASE(d3d);

	return hResult;
}


//
// Load and populate D3D9 texture from a PNG/GIF/JPEG file. 
// Return D3D texture and the original size of the loaded image file (width, height).
//
HRESULT D3D9LoadTextureFromFile(const LPDIRECT3DDEVICE9 pD3Device, const std::wstring& filePath, IDirect3DTexture9** pOutTexture, SIZE* pOutImageSize)
{
	if (filePath.empty() || pD3Device == nullptr || pOutTexture == nullptr || pOutImageSize == nullptr)
		return E_INVALIDARG;

	HRESULT hResult;
	HRESULT hResultCoInit = E_INVALIDARG;

	IWICImagingFactory* factory = nullptr;
	IWICBitmapDecoder* decoder = nullptr;
	IWICBitmapFrameDecode* frame = nullptr;
	IWICStream* stream = nullptr;
	IWICFormatConverter* formatConverter = nullptr;

	LPBYTE imgBuffer = nullptr;
	IDirect3DTexture9* texture = nullptr;

	try
	{
		hResultCoInit = CoInitialize(nullptr);

		hResult = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));

		if (SUCCEEDED(hResult)) hResult = factory->CreateStream(&stream);
		if (SUCCEEDED(hResult)) hResult = stream->InitializeFromFilename(filePath.c_str(), GENERIC_READ);
		if (SUCCEEDED(hResult)) hResult = factory->CreateDecoderFromStream(stream, NULL, WICDecodeMetadataCacheOnLoad, &decoder);
		if (SUCCEEDED(hResult)) hResult = decoder->GetFrame(0, &frame);
		if (SUCCEEDED(hResult)) hResult = factory->CreateFormatConverter(&formatConverter);
		if (SUCCEEDED(hResult)) hResult = formatConverter->Initialize(frame, GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom);

		if (SUCCEEDED(hResult)) frame->GetSize((UINT*)&pOutImageSize->cx, (UINT*)&pOutImageSize->cy);
		if (SUCCEEDED(hResult))
		{
			imgBuffer = new byte[pOutImageSize->cx * pOutImageSize->cy * 4];
			hResult = formatConverter->CopyPixels(nullptr, pOutImageSize->cx * 4, pOutImageSize->cx * pOutImageSize->cy * 4, imgBuffer);
		}

		// Create empty IDirect3DTexture9 at first and then populate it with bitmap data read from a file
		if (SUCCEEDED(hResult)) hResult = pD3Device->CreateTexture(pOutImageSize->cx, pOutImageSize->cy, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &texture, NULL);
		if (SUCCEEDED(hResult))
		{
			D3DLOCKED_RECT rect;
			hResult = texture->LockRect(0, &rect, 0, D3DLOCK_DISCARD);
			unsigned char* dest = static_cast<unsigned char*>(rect.pBits);			
			memcpy(dest, imgBuffer, sizeof(unsigned char) * pOutImageSize->cx * pOutImageSize->cy * 4);
			hResult = texture->UnlockRect(0);
		}
	}
	catch (...)
	{
		// Hmmmm.. Something went wrong...
		hResult = E_INVALIDARG;
	}

	if (SUCCEEDED(hResult))
	{
		// Caller is responsible to release the returned D3D9 texture object
		*pOutTexture = texture;
	}
	else
	{
		// Failed to populate the texture. Release it (if created) and don't return any texture obj to a caller
		SAFE_RELEASE(texture);
	}

	SAFE_RELEASE(stream);
	SAFE_RELEASE(formatConverter);
	SAFE_RELEASE(frame);
	SAFE_RELEASE(decoder);
	SAFE_RELEASE(factory);
	if (imgBuffer != nullptr) delete[] imgBuffer;

	if (hResultCoInit == S_OK) CoUninitialize();

	return hResult;
}


CUSTOM_VERTEX_TEX_2D D3D9CreateCustomVertexTex2D(float x, float y, DWORD color, float tu, float tv)
{
	CUSTOM_VERTEX_TEX_2D vertex{};
	
	vertex.x = x;
	vertex.y = y;
	vertex.z = 0.0f;
	vertex.rhw = 1.0f;
	vertex.color = color;
	vertex.tu = tu;
	vertex.tv = tv;

	return vertex;
}

CUSTOM_VERTEX_2D D3D9CreateCustomVertex2D(float x, float y, DWORD color)
{
	CUSTOM_VERTEX_2D vertex{};

	vertex.x = x;
	vertex.y = y;
	vertex.z = 0.0f;
	vertex.rhw = 1.0f;
	vertex.color = color;

	return vertex;
}


//
// Create D3D9 texture rectangle to be drawn at specified x/y location with cx/cy size
//
HRESULT D3D9CreateRectangleVertexTex2D(float x, float y, float cx, float cy, CUSTOM_VERTEX_TEX_2D* pOutVertexes2D, int iVertexesSize, DWORD color = D3DCOLOR_ARGB(255, 255, 255, 255))
{
	if (pOutVertexes2D == nullptr || iVertexesSize < sizeof(CUSTOM_VERTEX_TEX_2D) * 4)
		return E_INVALIDARG;

	// Create rectangle vertex of specified size at x/y position
	pOutVertexes2D[0] = D3D9CreateCustomVertexTex2D(x, y,       color /*D3DCOLOR_ARGB(255, 255, 255, 255)*/, 0.0f, 0.0f);
	pOutVertexes2D[1] = D3D9CreateCustomVertexTex2D(x+cx, y,    color /*D3DCOLOR_ARGB(255, 255, 255, 255)*/, 1.0f, 0.0f);
	pOutVertexes2D[2] = D3D9CreateCustomVertexTex2D(x, y+cy,    color /*D3DCOLOR_ARGB(255, 255, 255, 255)*/, 0.0f, 1.0f);
	pOutVertexes2D[3] = D3D9CreateCustomVertexTex2D(cx+x, cy+y, color /*D3DCOLOR_ARGB(255, 255, 255, 255)*/, 1.0f, 1.0f);
	
	return S_OK;
}


/*
CUSTOM_VERTEX_3D Create_Custom_Vertex_3D(float x, float y, float z, DWORD color, float tu, float tv)
{
	CUSTOM_VERTEX_3D vertex;

	vertex.x = x;
	vertex.y = y;
	vertex.z = z;
	vertex.color = color;
	vertex.tu = tu;
	vertex.tv = tv;

	return vertex;
}
*/

//
// Create D3D9 texture rectangle to be drawn at specified x/y location with cx/cy size.
//
HRESULT D3D9CreateRectangleVertex2D(float x, float y, float cx, float cy, CUSTOM_VERTEX_2D* pOutVertexes2D, int iVertexesSize, DWORD color /*= D3DCOLOR_ARGB(60, 255, 255, 255)*/)
{
	if (pOutVertexes2D == nullptr || iVertexesSize < sizeof(CUSTOM_VERTEX_2D) * 4)
		return E_INVALIDARG;

	// Create rectangle vertex of specified size at x/y position (used to highlight screenshot cropping area, so by default use semi-transparent color)
	pOutVertexes2D[0] = D3D9CreateCustomVertex2D(x, y,			 color /*D3DCOLOR_ARGB(60, 255, 255, 255)*/);
	pOutVertexes2D[1] = D3D9CreateCustomVertex2D(x + cx, y,		 color /*D3DCOLOR_ARGB(60, 255, 255, 255)*/);
	pOutVertexes2D[2] = D3D9CreateCustomVertex2D(x, y + cy,		 color /*D3DCOLOR_ARGB(60, 255, 255, 255)*/);
	pOutVertexes2D[3] = D3D9CreateCustomVertex2D(cx + x, cy + y, color /*D3DCOLOR_ARGB(60, 255, 255, 255)*/);

	return S_OK;
}


//
// Create D3D9 textured vertex buffer with a rect shape where texture is load from an external PNG/DDS/GIF/BMP file
//
HRESULT D3D9CreateRectangleVertexTexBufferFromFile(const LPDIRECT3DDEVICE9 pD3Device, const std::wstring& fileName, float x, float y, float cx, float cy, IMAGE_TEXTURE* pOutImageTexture, DWORD dwFlags)
{
	IDirect3DTexture9* pTempTexture = nullptr;

	if (pD3Device == nullptr || pOutImageTexture == nullptr || fileName.empty())
		return E_INVALIDARG;

	// Release existing texture and load a new texture from a file
	SAFE_RELEASE(pOutImageTexture->pTexture);

	if (!fs::exists(fileName))
		return E_INVALIDARG;

	pOutImageTexture->x = x;
	pOutImageTexture->y = y;
	pOutImageTexture->cx = cx;
	pOutImageTexture->cy = cy;
	pOutImageTexture->dwImageFlags = dwFlags;

	HRESULT hResult = D3D9LoadTextureFromFile(pD3Device, fileName, &pTempTexture, &pOutImageTexture->imgSize);
	if (SUCCEEDED(hResult))
	{		
		if ((cx == 0 && cy == 0) || (dwFlags & IMAGE_TEXTURE_SCALE_PRESERVE_ORIGSIZE))
		{
			// If cx/cy is all zero then use the original image size instead of the supplied custom size (re-scaled image rect texture)
			if ((dwFlags & IMAGE_TEXTURE_POSITION_HORIZONTAL_CENTER) && cx > (float)pOutImageTexture->imgSize.cx)
				x = x + ((cx - (float)pOutImageTexture->imgSize.cx) / 2.0f);

			if ((dwFlags & IMAGE_TEXTURE_POSITION_VERTICAL_CENTER) && cy > (float)pOutImageTexture->imgSize.cy)
				y = y + ((cy - (float)pOutImageTexture->imgSize.cy) / 2.0f);

			hResult = D3D9CreateRectangleVertexTex2D(x, y, (float)pOutImageTexture->imgSize.cx, (float)pOutImageTexture->imgSize.cy, pOutImageTexture->vertexes2D, sizeof(pOutImageTexture->vertexes2D));
		}
		else if (dwFlags & IMAGE_TEXTURE_SCALE_PRESERVE_ASPECTRATIO && pOutImageTexture->imgSize.cx > 0)
		{
			// Scale picture while keeping aspect ratio correct. Optionally place the image on the bottom of the specified rect area (x,y) (x+cx,y+cy)
			float scale_factor = cx / ((float)pOutImageTexture->imgSize.cx);
			float scaled_cy = ((float)pOutImageTexture->imgSize.cy) * scale_factor;
			float scaled_cx = cx;
			float scaled_x = x;
			float scaled_y = y;

			if (scaled_cy > cy)
			{
				scale_factor = cy / ((float)pOutImageTexture->imgSize.cy);
				scaled_cx = ((float)pOutImageTexture->imgSize.cx) * scale_factor;
				scaled_cy = cy;
			}

			if (dwFlags & IMAGE_TEXTURE_POSITION_BOTTOM)
				scaled_y = (y + cy) - scaled_cy;
			else if (dwFlags & IMAGE_TEXTURE_POSITION_VERTICAL_CENTER)
				scaled_y = y + ((cy - scaled_cy) / 2.0f);

			if (dwFlags & IMAGE_TEXTURE_POSITION_HORIZONTAL_CENTER)
				scaled_x = x + ((cx - scaled_cx) / 2.0f);
			else if (dwFlags & IMAGE_TEXTURE_POSITION_HORIZONTAL_RIGHT)
				scaled_x = (x + cx) - scaled_cx;

			hResult = D3D9CreateRectangleVertexTex2D(scaled_x, scaled_y, scaled_cx, scaled_cy, pOutImageTexture->vertexes2D, sizeof(pOutImageTexture->vertexes2D));
		}
		else
			// Re-scale the image to fill the target area
			hResult = D3D9CreateRectangleVertexTex2D(x, y, cx, cy, pOutImageTexture->vertexes2D, sizeof(pOutImageTexture->vertexes2D));
	}

	// If image file loading and texture/vertex creation failed then release the texture because it may be in invalid state
	if (SUCCEEDED(hResult))
		pOutImageTexture->pTexture = pTempTexture; // Caller's responsibility to release the texture
	else
		SAFE_RELEASE(pTempTexture);

	return hResult;
}


//
// Create D3D9 2D graphical rectangle vertex buffer (no texture, fill color alpha-transparent or opaque)
// 
HRESULT D3D9CreateRectangleVertexBuffer(const LPDIRECT3DDEVICE9 pD3Device, float x, float y, float cx, float cy, LPDIRECT3DVERTEXBUFFER9* pOutVertexBuffer, DWORD color)
{
	HRESULT hResult;
	LPDIRECT3DVERTEXBUFFER9 pTempVertexBuffer = nullptr;
	CUSTOM_VERTEX_2D custRectVertex[4];

	if (pD3Device == nullptr || pOutVertexBuffer == nullptr)
		return E_INVALIDARG;

	SAFE_RELEASE( (*pOutVertexBuffer) );

	hResult = pD3Device->CreateVertexBuffer(sizeof(custRectVertex), 0, CUSTOM_VERTEX_FORMAT_2D, D3DPOOL_DEFAULT, &pTempVertexBuffer, nullptr);
	if (SUCCEEDED(hResult))
	{		
		void* pData = nullptr;

		hResult = D3D9CreateRectangleVertex2D(x, y, cx, cy, custRectVertex, sizeof(custRectVertex), color);

		if (SUCCEEDED(hResult)) hResult = pTempVertexBuffer->Lock(0, 0, (void**)&pData, 0);
		if (SUCCEEDED(hResult)) memcpy(pData, custRectVertex, sizeof(custRectVertex));
		if (SUCCEEDED(hResult)) hResult = pTempVertexBuffer->Unlock();

		if (SUCCEEDED(hResult))
			*pOutVertexBuffer = pTempVertexBuffer; // Caller's responsibility to release the returned vertex buffer object
		else
			SAFE_RELEASE(pTempVertexBuffer);
	}

	return hResult;
}


// 
// Create a DX9 vertex buffer to for a colored circle
//
// The base of the code derived from https://stackoverflow.com/questions/21330429/draw-point-or-filled-in-circle (posted by Typ1232)
// Modified here for NGPCarMenu plugin purposes by mika-n.
//
/*
#define CIRCLE_RESOLUTION 64
HRESULT D3D9CreateCircleVertexBuffer(const LPDIRECT3DDEVICE9 pD3Device, float mx, float my, float r, LPDIRECT3DVERTEXBUFFER9* pOutVertexBuffer, DWORD color)
{
	HRESULT hResult;
	LPDIRECT3DVERTEXBUFFER9 pTempVertexBuffer = nullptr;
	CUSTOM_VERTEX_2D circleVertexes[CIRCLE_RESOLUTION + 1];

	if (pD3Device == nullptr || pOutVertexBuffer == nullptr)
		return E_INVALIDARG;

	SAFE_RELEASE((*pOutVertexBuffer));

	hResult = pD3Device->CreateVertexBuffer(sizeof(circleVertexes), 0, CUSTOM_VERTEX_FORMAT_2D, D3DPOOL_DEFAULT, &pTempVertexBuffer, nullptr);
	if (SUCCEEDED(hResult))
	{
		void* pData;

		for (int i = 0; i < CIRCLE_RESOLUTION + 1; i++)
		{
			circleVertexes[i].x = mx + r * cos(D3DX_PI * (i / (CIRCLE_RESOLUTION / 2.0f)));
			circleVertexes[i].y = my + r * sin(D3DX_PI * (i / (CIRCLE_RESOLUTION / 2.0f)));
			circleVertexes[i].z = 0;
			circleVertexes[i].rhw = 1;
			circleVertexes[i].color = color;
		}

		if (SUCCEEDED(hResult)) hResult = pTempVertexBuffer->Lock(0, 0, (void**)&pData, 0);
		if (SUCCEEDED(hResult)) memcpy(pData, circleVertexes, sizeof(circleVertexes));
		if (SUCCEEDED(hResult)) hResult = pTempVertexBuffer->Unlock();

		if (SUCCEEDED(hResult))
			*pOutVertexBuffer = pTempVertexBuffer; // Caller's responsibility to release the returned vertex buffer object
		else
			SAFE_RELEASE(pTempVertexBuffer);
	}

	return hResult;
}
*/


#define CIRCLE_RESOLUTION 10
void D3D9DrawPrimitiveCircle(const LPDIRECT3DDEVICE9 pD3Device, float mx, float my, float r, DWORD color)
{
	CUSTOM_VERTEX_2D circleVertexes[CIRCLE_RESOLUTION];

	for (int i = 0; i < CIRCLE_RESOLUTION; i++)
	{
		circleVertexes[i].x = mx + r * cosf(D3DX_PI * (i / (CIRCLE_RESOLUTION / 2.0f)));
		circleVertexes[i].y = my + r * sinf(D3DX_PI * (i / (CIRCLE_RESOLUTION / 2.0f)));
		circleVertexes[i].z = 0;
		circleVertexes[i].rhw = 1;
		circleVertexes[i].color = color;
	}

	pD3Device->SetFVF(CUSTOM_VERTEX_FORMAT_2D /*D3DFVF_XYZRHW | D3DFVF_DIFFUSE*/);
	pD3Device->DrawPrimitiveUP(D3DPT_TRIANGLEFAN, CIRCLE_RESOLUTION-2, &circleVertexes, sizeof(CUSTOM_VERTEX_2D));
}


//
// Draw D3D9 texture vertex (usually shape of rectangle) and use a supplied custom texture
//
void D3D9DrawVertexTex2D(const LPDIRECT3DDEVICE9 pD3Device, IDirect3DTexture9* pTexture, const CUSTOM_VERTEX_TEX_2D* vertexes2D, CD3D9RenderStateCache* pRenderStateCache)
{
	// Caller's responsibility to make sure the pD3Device and texture and vertexes2D parameters are not NULL (this method 
	// may be called several times per frame, so no need to do extra paranoid checks each time).
	if (pRenderStateCache == nullptr)
	{
		pD3Device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
		pD3Device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
		pD3Device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
		pD3Device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
		pD3Device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
		pD3Device->SetTextureStageState(0, D3DTSS_CONSTANT, D3DCOLOR_ARGB(255, 255, 255, 255));
		pD3Device->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_CONSTANT);
		//pD3Device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
		//pD3Device->SetRenderState(D3DRS_AMBIENT, D3DCOLOR_ARGB(255, 255, 255, 255));
		pD3Device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
	}
	else
	{
		// Render state cache defined. Set stage and state changes via a cache,so caller can restore all DX9 stage changes
		pRenderStateCache->SetTextureStageState(D3DTSS_COLOROP, D3DTOP_MODULATE);
		pRenderStateCache->SetTextureStageState(D3DTSS_COLORARG1, D3DTA_TEXTURE);
		pRenderStateCache->SetTextureStageState(D3DTSS_COLORARG2, D3DTA_DIFFUSE);
		pRenderStateCache->SetTextureStageState(D3DTSS_ALPHAOP, D3DTOP_MODULATE);
		pRenderStateCache->SetTextureStageState(D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
		pRenderStateCache->SetTextureStageState(D3DTSS_CONSTANT, D3DCOLOR_ARGB(255, 255, 255, 255));
		pRenderStateCache->SetTextureStageState(D3DTSS_ALPHAARG2, D3DTA_CONSTANT);
		//pRenderStateCache->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
		//pRenderStateCache->SetRenderState(D3DRS_AMBIENT, D3DCOLOR_ARGB(255, 255, 255, 255));
		pRenderStateCache->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
	}
	
	pD3Device->SetFVF(CUSTOM_VERTEX_FORMAT_TEX_2D);
	pD3Device->SetTexture(0, pTexture);
	pD3Device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, vertexes2D, sizeof(CUSTOM_VERTEX_TEX_2D));
	pD3Device->SetTexture(0, nullptr);
}


//
// Draw a vertex without a texture (usually rectangle) using alpa blending transparent color
//
void D3D9DrawVertex2D(const LPDIRECT3DDEVICE9 pD3Device, const LPDIRECT3DVERTEXBUFFER9 pVertexBuffer)
{
	// Caller's responsible to make sure the buffer is not NULL

	pD3Device->SetStreamSource(0, pVertexBuffer, 0, sizeof(CUSTOM_VERTEX_2D));
	pD3Device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
	pD3Device->SetFVF(CUSTOM_VERTEX_FORMAT_2D);
	pD3Device->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);
}


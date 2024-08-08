/**
 * MIT License
 *
 * Copyright (c) 2024 Victor Moncada <vtr.moncada@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "micro/bits.hpp"

#if defined( _MSC_VER ) || defined(__MINGW32__)
#define MICRO_WIN32_API 1
#else
#define MICRO_WIN32_API 0
#endif

#if MICRO_WIN32_API
#include <windows.h> 
#include <tchar.h>
#include <strsafe.h>
#else
// For PATH_MAX
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 1
#endif
#include <limits.h>
#endif

#include <cstdio> 
#include <string>
#include <cstdio>
#include <cstdarg>
#include <cstdlib>
#include <vector>



static int ErrorExit(const char* text, ...)
{
    va_list args;
    va_start(args, text);
    vfprintf(stderr, text, args);
    va_end(args);
    return -1;
}

static int Replace(std::string& in, const char* _from, const char* _to)
{
    std::string& str = in;
    std::string from = _from;
    std::string to = _to;
    int res = 0;
    size_t start_pos = 0;
    while ((start_pos = str.find(from, start_pos)) != std::string::npos)
    {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length(); // Handles case where 'to' is a substring of 'from'
        ++res;
    }
    return res;
}

static std::string GetProcessPath(const char* full_file_path)
{
    std::string app_path = full_file_path;
    Replace(app_path, "\\", "/");
    size_t pos = app_path.find_last_of("/");
    if (pos != std::string::npos) {
        app_path = app_path.substr(0, pos + 1);
    }
    else
        app_path.clear();
    return app_path;
}


static std::string RemoveQuotes(const char* arg)
{
    std::string s = arg;
    if (s.find("\"") == 0 && s.find_last_of("\"") == s.size() - 1 && s.size() > 1)
        return s.substr(1, s.size() - 2);
    if (s.find("'") == 0 && s.find_last_of("'") == s.size() - 1 && s.size() > 1)
        return s.substr(1, s.size() - 2);
    return s;
}

int main(int argc, char** argv)
{

	if (argc == 1) {
		return ErrorExit("Empty command line!!");
	}

	// Read env. variables
	int start = 1;
	std::vector<std::string> envs;
	for (; start < argc; ++start) {
		std::string c = RemoveQuotes(argv[start]);
		if (c.find("MICRO_") == 0) {
			envs.push_back(c);
		}
		else {
			break;
		}
	}

	for (std::string& env : envs) {
		putenv((char*)env.c_str());
	}

	// Read full command
	std::string cmd;
	for (; start < argc; ++start) {
		if (!cmd.empty())
			cmd += " ";
		cmd += argv[start];
	}

#if MICRO_WIN32_API

	// Set up members of the PROCESS_INFORMATION structure.
	PROCESS_INFORMATION piProcInfo;
	STARTUPINFO siStartInfo;
	BOOL bSuccess = FALSE;
	void* page = nullptr;

	ZeroMemory(&piProcInfo, sizeof(PROCESS_INFORMATION));
	ZeroMemory(&siStartInfo, sizeof(STARTUPINFO));
	siStartInfo.cb = sizeof(STARTUPINFO);
	siStartInfo.hStdError = GetStdHandle(STD_ERROR_HANDLE);
	siStartInfo.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
	siStartInfo.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
	siStartInfo.dwFlags |= STARTF_USESTDHANDLES;

	// Create the child process.

	bSuccess = CreateProcess(nullptr, (char*)cmd.c_str(), nullptr, nullptr, TRUE, CREATE_SUSPENDED, nullptr, nullptr, &siStartInfo, &piProcInfo);

	if (!bSuccess)
		return ErrorExit("Failed to create child process, error code = 0x%08X\n", GetLastError());

	// Start Inject dll /////////////////////////////////////////////////////////////////////////////////////

	// Verify path length.
	std::string lib_path = GetProcessPath(argv[0]);
	lib_path += "micro_proxy.dll"; // TEST: introduce error
	size_t len = lib_path.size() + 1;
	if (len > MAX_PATH) {
		return ErrorExit("path length (%d) exceeds MAX_PATH (%d).\n", len, MAX_PATH);
	}

	if (GetFileAttributes(lib_path.c_str()) == INVALID_FILE_ATTRIBUTES) {
		return ErrorExit("unable to locate library (%s).\n", lib_path.c_str());
	}

	// Allocate a page in memory for the arguments of LoadLibrary.
	page = VirtualAllocEx(piProcInfo.hProcess, nullptr, MAX_PATH, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	if (page == nullptr) {
		return ErrorExit("VirtualAllocEx failed; error code = 0x%08X\n", GetLastError());
	}

	// Write library path to the page used for LoadLibrary arguments.
	if (WriteProcessMemory(piProcInfo.hProcess, page, lib_path.c_str(), len, nullptr) == 0) {
		return ErrorExit("WriteProcessMemory failed; error code = 0x%08X\n", GetLastError());
	}

	// Inject the shared library into the address space of the process,
	// through a call to LoadLibrary.
	HANDLE hThread = CreateRemoteThread(piProcInfo.hProcess, nullptr, 0, (LPTHREAD_START_ROUTINE)LoadLibraryA, page, 0, nullptr);
	if (hThread == nullptr) {
		return ErrorExit("CreateRemoteThread failed; error code = 0x%08X\n", GetLastError());
	}

	// Wait for DllMain to return.
	if (WaitForSingleObject(hThread, INFINITE) == WAIT_FAILED) {
		return ErrorExit("WaitForSingleObject failed; error code = 0x%08X\n", GetLastError());
	}

	// Cleanup.
	CloseHandle(hThread);

	// Resume
	if (ResumeThread(piProcInfo.hThread) == -1) {
		return ErrorExit("ResumeThread failed; error code = 0x%08X\n", GetLastError());
	}

	// End Inject dll /////////////////////////////////////////////////////////////////////////////////////

	// Wait for DllMain to return.
	if (WaitForSingleObject(piProcInfo.hProcess, INFINITE) == WAIT_FAILED) {
		return ErrorExit("WaitForSingleObject failed; error code = 0x%08X\n", GetLastError());
	}

	DWORD exit_code = 0;
	if (FALSE == GetExitCodeProcess(piProcInfo.hProcess, &exit_code)) {
		// nothing to do
		bool stop = true;
	}

	// Close handles to the child process and its primary thread.
	// Some applications might keep these handles to monitor the status
	// of the child process, for example.

	CloseHandle(piProcInfo.hProcess);
	CloseHandle(piProcInfo.hThread);

	// Cleanup
	if (page)
		VirtualFreeEx(piProcInfo.hProcess, page, MAX_PATH, MEM_RELEASE);

	return exit_code;
#else

	// For all other OS, use LD_PRELOAD trick (or similar)

	char buffer[PATH_MAX];
	memset(buffer, 0, sizeof(buffer));
	realpath(argv[0], buffer);

#ifndef __APPLE__
    
	std::string env = "LD_PRELOAD=" + GetProcessPath(buffer) + "libmicro_proxy.so";
	putenv((char*)env.c_str());
	std::system(cmd.c_str());
#else
	std::string env = "DYLD_INSERT_LIBRARIES=" + GetProcessPath(buffer) + "libmicro_proxy.dylib";
	putenv((char*)env.c_str());
	std::system(cmd.c_str());
#endif

#endif
}

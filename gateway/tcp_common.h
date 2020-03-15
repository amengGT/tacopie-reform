#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include <tacopie/tacopie>
#ifdef _WIN64
#ifndef _DEBUG
#pragma comment(lib,"../X64/Release/tacopie.lib")
#else
#pragma comment(lib,"../X64/Debug/tacopie.lib")
#endif
#else
#ifndef _DEBUG
#pragma comment(lib,"../Win32/Release/tacopie.lib")
#else
#pragma comment(lib,"../Win32/Debug/tacopie.lib")
#endif
#endif

#include <WS2tcpip.h>
#pragma warning(disable:4996)

#include <map>
#include <iostream>
using namespace std;
using namespace tacopie;

#define TCP_BUFF_LEN				16384

extern std::uint32_t g_connect_time_out;
extern std::uint32_t g_local_port;
extern std::uint32_t g_remote_port;
extern std::string g_remote_host;
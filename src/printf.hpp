#pragma once

#ifdef _MSC_VER && _DEBUG
	// workaround to make printf work for Win32 desktop applications under MSVC debugger
	#define WIN32_LEAN_AND_MEAN
	#include <windows.h>
	#include <strsafe.h>
	#define printf(kwszDebugFormatString, ...) \
		_DBGPRINT(kwszDebugFormatString, __VA_ARGS__)

VOID _DBGPRINT(LPCSTR kwszDebugFormatString, ...) {
	INT cbFormatString = 0;
	va_list args;
	PCHAR szDebugString = NULL;
	
	va_start(args, kwszDebugFormatString);
	cbFormatString = _vscprintf(kwszDebugFormatString, args) * sizeof(CHAR) + 1;
	szDebugString = (PCHAR)_malloca(cbFormatString);
	StringCbVPrintfA(szDebugString, cbFormatString,
	                 kwszDebugFormatString, args);
	OutputDebugStringA(szDebugString);
	_freea(szDebugString);
	va_end(args);
}
#else
	#define _DBGPRINT(kwszDebugFormatString, ...) ;;
#endif // _MSC_VER else


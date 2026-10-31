#include "stdafx.h"
#include "MiniDump.h"
#if defined( NV_WIN_PLATFORM )

#include "LogFileName.h"
#include "FileSystem/FileUtils.h"
#include "TimeUtils.h"
#include "Vendor/DTW/inc/dbghelp.h"
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static const MINIDUMP_TYPE miniMiniDumpFlags = (MINIDUMP_TYPE)( MiniDumpWithIndirectlyReferencedMemory );
static const MINIDUMP_TYPE fullMiniDumpFlags = (MINIDUMP_TYPE)( MiniDumpWithDataSegs | MiniDumpWithThreadInfo |
																															 MiniDumpWithFullMemoryInfo | MiniDumpWithProcessThreadData |
																															 MiniDumpWithHandleData | MiniDumpWithPrivateReadWriteMemory );
static MINIDUMP_EXCEPTION_INFORMATION dumpExceptionInfo;

DEV_VAR_STATIC bool g_fullDump = false;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CreateMiniDump( const struct tm &tim, EXCEPTION_POINTERS* ExceptionInfo, MiniDumpType dumpFlags, ExceptionType exceptionType )
{
	static char desc[64];
  
  if ( g_fullDump )
    dumpFlags = FULL_DUMP;

	sprintf_s( desc, "%s%s", exceptionType == EXCEPTION_ASSERT ? "assert" : "exception", dumpFlags == MINI_DUMP ? "" : "-full" );

	static ptrdiff_t dumpFileHandle;
	dumpFileHandle = NFile::CreateFileWithDir( NDebug::GenerateDebugFileName( tim, desc, "dmp" ).c_str() );
	//dumpFileHandle = CreateFile( NDebug::GenerateDebugFileName( time, desc, "dmp" ).c_str(), GENERIC_WRITE, 0, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0 );

	if( dumpFileHandle != (ptrdiff_t)INVALID_HANDLE_VALUE ) //just to avoid any exception
	{
		dumpExceptionInfo.ThreadId = GetCurrentThreadId();
		dumpExceptionInfo.ExceptionPointers	= ExceptionInfo;
		dumpExceptionInfo.ClientPointers	= true;

		MiniDumpWriteDump( GetCurrentProcess(), GetCurrentProcessId(), (HANDLE)dumpFileHandle
			, dumpFlags == MINI_DUMP ? miniMiniDumpFlags : fullMiniDumpFlags
			,	&dumpExceptionInfo, NULL, NULL );
		CloseHandle( (HANDLE)dumpFileHandle );
	}
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Minidump from the first-chance vectored handler (PW_Game.cpp FastfailVeh).
// Some fatal exceptions never reach ExceptionFilter: the process just disappears without
// exception.dmp/log. A vectored handler sees them first, but MiniDumpWriteDump must not run
// on the faulting thread (a stack overflow leaves no stack, the thread may hold the heap or
// loader lock), so a thread created at startup writes <log dir>\...-fatal.dmp while the
// faulting thread waits. A handled first-chance exception also produces such a dump: the
// launcher sends it only when the game then exits with a crash code and deletes it after a
// normal exit. At most FATAL_DUMP_LIMIT per process, none within FATAL_DUMP_QUIET_MS after
// the previous one (the unhandled filter probes memory with IsBadCodePtr right after a crash).
enum
{
  FATAL_DUMP_LIMIT = 3,
  FATAL_DUMP_QUIET_MS = 5000,
  FATAL_DUMP_WAIT_MS = 60000
};

static HANDLE s_fatalRequest = 0;
static HANDLE s_fatalDone = 0;
static DWORD s_fatalThreadId = 0;
static EXCEPTION_POINTERS * volatile s_fatalPointers = 0;
static volatile DWORD s_fatalFaultThread = 0;
static volatile LONG s_fatalBusy = 0;
static volatile LONG s_fatalCount = 0;
static volatile DWORD s_fatalLastTick = 0;

static void WriteFatalDumpFile()
{
  struct tm tim;
  GetOsUtcTime( &tim );
  const nstl::string fileName = NDebug::GenerateDebugFileName( tim, "fatal", "dmp" );
  HANDLE file = (HANDLE)NFile::CreateFileWithDir( fileName.c_str() );
  if ( file == INVALID_HANDLE_VALUE )
    return;

  MINIDUMP_EXCEPTION_INFORMATION info;
  info.ThreadId = s_fatalFaultThread;
  info.ExceptionPointers = s_fatalPointers;
  info.ClientPointers = FALSE;
  MiniDumpWriteDump( GetCurrentProcess(), GetCurrentProcessId(), file, miniMiniDumpFlags, &info, NULL, NULL );
  CloseHandle( file );
}

static void WriteFatalDumpGuarded()
{
  __try
  {
    WriteFatalDumpFile();
  }
  __except ( EXCEPTION_EXECUTE_HANDLER )
  {
  }
}

static DWORD WINAPI FatalDumpThread( LPVOID )
{
  while ( WaitForSingleObject( s_fatalRequest, INFINITE ) == WAIT_OBJECT_0 )
  {
    WriteFatalDumpGuarded();
    SetEvent( s_fatalDone );
  }
  return 0;
}

extern "C" void PwStartFatalDumpThread()
{
  if ( s_fatalRequest )
    return;
  s_fatalRequest = CreateEvent( 0, FALSE, FALSE, 0 );
  s_fatalDone = CreateEvent( 0, FALSE, FALSE, 0 );
  if ( !s_fatalRequest || !s_fatalDone )
    return;
  HANDLE thread = CreateThread( 0, 256 * 1024, FatalDumpThread, 0, 0, &s_fatalThreadId );
  if ( thread )
    CloseHandle( thread );
  else
    s_fatalThreadId = 0;
}

extern "C" void PwWriteFatalDump( EXCEPTION_POINTERS * pointers )
{
  if ( !s_fatalThreadId || GetCurrentThreadId() == s_fatalThreadId )
    return;
  if ( s_fatalCount >= FATAL_DUMP_LIMIT )
    return;
  if ( s_fatalLastTick && GetTickCount() - s_fatalLastTick < FATAL_DUMP_QUIET_MS )
    return;
  // another thread is being dumped right now
  if ( InterlockedCompareExchange( &s_fatalBusy, 1, 0 ) != 0 )
    return;

  InterlockedIncrement( &s_fatalCount );
  s_fatalPointers = pointers;
  s_fatalFaultThread = GetCurrentThreadId();
  ResetEvent( s_fatalDone );
  SetEvent( s_fatalRequest );
  WaitForSingleObject( s_fatalDone, FATAL_DUMP_WAIT_MS );

  const DWORD tick = GetTickCount();
  s_fatalLastTick = tick ? tick : 1;
  InterlockedExchange( &s_fatalBusy, 0 );
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
REGISTER_DEV_VAR( "full_dump", g_fullDump, STORAGE_NONE );
#endif // NV_WIN_PLATFORM

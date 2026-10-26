#include "stdafx.h"
#include "../System/HPTimer.h"
#include "../Render/renderer.h"

static int g_externalSyncCount = 1;
static float g_timeShift = 1.0f;
REGISTER_DEV_VAR( "sync_frame_count", g_externalSyncCount, STORAGE_NONE );
static NDebug::DebugVar<float> vsync_Delta( "Delta", "PerfCnt" );

namespace NMainLoop
{
namespace VSyncController
{

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
namespace
{
	NHPTimer::STime startTime = 0;
	int   syncFrameNumber = 1; // Sync every syncFrameNumber frame


	float MeasureTimeDelta()
	{
		NHPTimer::STime endTime;
		NHPTimer::GetTime(endTime);
		endTime -= startTime;
		return float(NHPTimer::Time2Seconds(endTime));
	}

}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
float GetSyncTimeDelta()
{
	const Render::RenderMode &mode = Render::GetRenderer()->GetCurrentRenderMode();
	if (mode.vsyncCount > 0 && mode.refreshRate > 0)
		return 1.0f / float(mode.refreshRate); // Vsync is on
	else
		return 0.0f;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WaitBeforePresent()
{
  const float syncTimeDelta = GetSyncTimeDelta();

	// First frame?
	if (startTime == 0 || syncTimeDelta == 0.0f) 
		return;

	syncFrameNumber = g_externalSyncCount;

	float timeDelta  = MeasureTimeDelta();
	float time2Check = syncTimeDelta * syncFrameNumber;
	if (timeDelta < time2Check)
	{
		time2Check -= syncTimeDelta * (1.0f - g_timeShift);
		if (timeDelta < time2Check)
		{
			DebugTrace("Wait for %3.1f msecs to reach %3.1f", (time2Check - timeDelta) * 1000.f, time2Check * 1000.0f);
			do 
			{
				timeDelta = MeasureTimeDelta();
			} while (timeDelta < time2Check); // Wait for half of the frame
		}
	} // end if (timeDelta < time2Check)
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CalculatePresentCount()
{
  const float syncTimeDelta = GetSyncTimeDelta();

	// First frame?
	if (startTime == 0 || syncTimeDelta == 0.0f) 
		return 1;

	syncFrameNumber = g_externalSyncCount;

	float timeDelta  = MeasureTimeDelta();
	return Max(0, syncFrameNumber - int(ceil(timeDelta / syncTimeDelta))) + 1;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Frame rate limit (gfx_max_fps, 0 = off). Works only without vertical sync: with vsync the monitor sets the pace.
// The main loop waits before Present until the next frame slot, so the frames go out evenly.
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static int g_maxFps = 0;
REGISTER_VAR( "gfx_max_fps", g_maxFps, STORAGE_USER );

namespace
{
	const int c_minLimitFps = 20;   // lower values are raised to this one
	const int c_maxLimitFps = 1000;

	HANDLE CreateLimitTimer()
	{
		// CREATE_WAITABLE_TIMER_HIGH_RESOLUTION (Windows 10 1803+): sleeps for a fraction of a millisecond
		typedef HANDLE (WINAPI *CreateWaitableTimerExWFunc)( LPSECURITY_ATTRIBUTES, LPCWSTR, DWORD, DWORD );
		const DWORD highResolutionFlag = 0x00000002;
		CreateWaitableTimerExWFunc createTimer = (CreateWaitableTimerExWFunc)GetProcAddress( GetModuleHandleA( "kernel32.dll" ), "CreateWaitableTimerExW" );
		return createTimer ? createTimer( NULL, NULL, highResolutionFlag, TIMER_ALL_ACCESS ) : NULL;
	}

	// sleeps for about 'seconds' (less than asked rather than more)
	void SleepFor( double seconds )
	{
		static const HANDLE timer = CreateLimitTimer();
		if ( timer )
		{
			LARGE_INTEGER dueTime;
			dueTime.QuadPart = -LONGLONG( seconds * 1.0e7 ); // relative, 100 ns units
			if ( SetWaitableTimer( timer, &dueTime, 0, NULL, NULL, FALSE ) && WaitForSingleObject( timer, 100 ) == WAIT_OBJECT_0 )
				return;
		}
		// without the timer Sleep(1) lasts a timer tick (1 ms after timeBeginPeriod(1) in the renderer, else up to 15.6 ms)
		if ( seconds >= 0.016 )
			Sleep( DWORD( seconds * 1000.0 ) - 1 );
		else
			SwitchToThread();
	}

	NHPTimer::STime nextFrameTime = 0;
}

int GetMaxFps()
{
	if ( g_maxFps <= 0 )
		return 0;
	return Min( Max( g_maxFps, c_minLimitFps ), c_maxLimitFps );
}

void LimitFrameRate()
{
	const int maxFps = GetMaxFps();
	const Render::RenderMode &mode = Render::GetRenderer()->GetCurrentRenderMode();
	if ( maxFps <= 0 || mode.vsyncCount > 0 )
	{
		nextFrameTime = 0;
		return;
	}

	const NHPTimer::STime period = NHPTimer::Seconds2Time( 1.0 / maxFps );
	NHPTimer::STime now;
	NHPTimer::GetTime( now );

	// first frame, or the frame took longer than the slot: start counting from now (no catching up with a burst of frames)
	if ( nextFrameTime == 0 || now - nextFrameTime > period )
	{
		nextFrameTime = now + period;
		return;
	}

	for ( ;; )
	{
		const double left = NHPTimer::Time2Seconds( nextFrameTime - now );
		if ( left <= 0.0 )
			break;
		if ( left > 0.0015 )
			SleepFor( left - 0.001 ); // the last millisecond is waited actively: the timer may wake up late
		else
			YieldProcessor();
		NHPTimer::GetTime( now );
	}
	nextFrameTime += period;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void MarkPresentFinished()
{
	vsync_Delta.SetValue( float(NHPTimer::GetTimePassedAndUpdateTime(startTime)) * 1000.0f );
}

}
}
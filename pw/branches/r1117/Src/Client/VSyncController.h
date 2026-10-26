#pragma once

namespace NMainLoop
{

namespace VSyncController
{
	void WaitBeforePresent();
	int  CalculatePresentCount();
	void MarkPresentFinished();

	// gfx_max_fps: frame rate limit (0 = off), ignored while vsync is on
	int  GetMaxFps();
	void LimitFrameRate();
}


}
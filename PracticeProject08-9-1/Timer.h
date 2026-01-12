#pragma once
//-----------------------------------------------------------------------------
// File: CGameTimer.h
//-----------------------------------------------------------------------------

const ULONG MAX_SAMPLE_COUNT = 50; // Maximum frame time sample count

class CGameTimer
{
public:
	CGameTimer();
	virtual ~CGameTimer();

	void Tick(float fLockFPS = 0.0f);
	void Start();
	void Stop();
	void Reset();

    unsigned long GetFrameRate(LPTSTR lpszString = nullptr, int nCharacters = 0);
    float GetTimeElapsed();
	float GetTotalTime();

private:
	double							m_fTimeScale{ 0.0 };
	float							m_fTimeElapsed{ 0.0 };

	__int64							m_nBasePerformanceCounter{ 0 };
	__int64							m_nPausedPerformanceCounter{ 0 };
	__int64							m_nStopPerformanceCounter{ 0 };
	__int64							m_nCurrentPerformanceCounter{ 0 };
    __int64							m_nLastPerformanceCounter{ 0 };

	__int64							m_nPerformanceFrequencyPerSec{ 0 };

    float							m_fFrameTime[MAX_SAMPLE_COUNT]{ 0.0 };
    ULONG							m_nSampleCount{ 0 };

    unsigned long					m_nCurrentFrameRate{ 0 };
	unsigned long					m_nFramesPerSecond{ 0 };
	float							m_fFPSTimeElapsed{ 0.0 };

	bool							m_bStopped{ false };
};

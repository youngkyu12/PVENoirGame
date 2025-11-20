#pragma once

const ULONG MAX_SAMPLE_COUNT = 50;	// 50 회의 프레임 처리시간을 누적하여 평균한다.

class CGameTimer {
public:
	CGameTimer();
	virtual ~CGameTimer();

public:
	void Start() {}
	void Stop() {}
	void Reset();
	void Tick(float fLockFPS = 0.0f);
	unsigned long GetFrameRate(LPTSTR lpszString = NULL, int nCharacters = 0);
	float GetTimeElapsed();

private:
	bool m_bHardwareHasPerformanceCounter;
	float m_fTimeScale;
	float m_fTimeElapsed;
	__int64 m_nCurrentTime;
	__int64 m_nLastTime;
	__int64 m_nPerformanceFrequency;

	float m_fFrameTime[MAX_SAMPLE_COUNT];
	ULONG m_nSampleCount = 0;

	unsigned long m_nCurrentFrameRate = 0;
	unsigned long m_nFramesPerSecond = 0;
	float m_fFPSTimeElapsed = 0.0f;

	bool m_bStopped;
};


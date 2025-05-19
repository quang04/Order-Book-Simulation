#pragma once
#include <iostream>
#include <intrin.h>
#include <intrin0.h>

class SpinLock {
public:
	static inline void Delay(int _cpuCycle = 10000)
	{
		auto s = __rdtsc();
		while (__rdtsc() - s < _cpuCycle)
		{
			_mm_pause();
		}
	}
};
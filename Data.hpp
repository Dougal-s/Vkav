#pragma once
#ifndef DATA_HPP
#define DATA_HPP

#include <stddef.h>

struct AudioData {
	float* lBuffer;
	float* rBuffer;
	float* buffer;

	float lVolume = 0.f;
	float rVolume = 0.f;

	void allocate(size_t bufferSize, size_t lrBufferSize);

	void deallocate();
};

#endif

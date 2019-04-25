#include "Data.hpp"

void AudioData::allocate(size_t bufferSize, size_t lrBufferSize) {
	lBuffer = new float[lrBufferSize];
	rBuffer = new float[lrBufferSize];
	buffer = new float[bufferSize];
}

void AudioData::deallocate() {
	delete[] lBuffer;
	delete[] rBuffer;
	delete[] buffer;
}

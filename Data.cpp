#include "Data.hpp"

AudioData::AudioData() {
	lBuffer = new float[0];
	rBuffer = new float[0];
	buffer = new float[0];
}

void AudioData::allocate(size_t bufferSize, size_t lrBufferSize) {
	delete[] lBuffer;
	delete[] rBuffer;
	delete[] buffer;
	lBuffer = new float[lrBufferSize];
	rBuffer = new float[lrBufferSize];
	buffer = new float[bufferSize];
}

AudioData::~AudioData() {
	delete[] lBuffer;
	delete[] rBuffer;
	delete[] buffer;
}

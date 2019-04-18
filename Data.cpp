#include "Data.hpp"

void AudioData::allocate(size_t size) {
	lBuffer = new float[size/2];
	rBuffer = new float[size/2];
	buffer = new float[2*size];
}

void AudioData::deallocate() {
	delete[] lBuffer;
	delete[] rBuffer;
	delete[] buffer;
}

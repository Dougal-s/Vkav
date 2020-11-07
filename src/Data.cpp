#include <cstddef>

#include "Data.hpp"

AudioData::AudioData() {
	lBuffer = new float[0];
	rBuffer = new float[0];
	buffer = new float[0];
}

void AudioData::allocate(size_t channels, size_t channelSize) {
	delete[] lBuffer;
	delete[] rBuffer;
	delete[] buffer;
	lBuffer = new float[channelSize/2];
	rBuffer = new float[channelSize/2];
	buffer = new float[channels*channelSize];
}

AudioData::~AudioData() {
	delete[] lBuffer;
	delete[] rBuffer;
	delete[] buffer;
}

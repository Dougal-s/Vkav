#pragma once
#ifndef SIGNAL_FUNCTIONS_HPP
#define SIGNAL_FUNCTIONS_HPP

struct AudioData;

struct ProcessSettings {
	size_t size;
	float smoothingLevel;
	float amplitude;
	unsigned char channels;
};

class Process {
public:
	Process() = default;
	Process(const ProcessSettings& createInfo);
	~Process();

	Process& operator=(Process&& other) noexcept;

	void processSignal(AudioData& audio);

private:
	class ProcessImpl;
	ProcessImpl* impl = nullptr;
};

#endif

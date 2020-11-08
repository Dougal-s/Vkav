#pragma once
#ifndef SIGNAL_FUNCTIONS_HPP
#define SIGNAL_FUNCTIONS_HPP

struct AudioData;

class Process {
public:
	struct Settings {
		size_t size;
		float smoothingLevel;
		float amplitude;
		unsigned char channels;
	};

	Process() = default;
	Process(const Settings& settings);
	~Process();

	Process& operator=(Process&& other) noexcept;

	void processSignal(AudioData& audio);

private:
	class ProcessImpl;
	ProcessImpl* impl = nullptr;
};

#endif

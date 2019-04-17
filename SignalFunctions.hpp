#pragma once
#ifndef SIGNAL_FUNCTIONS_HPP
#define SIGNAL_FUNCTIONS_HPP
#include <vector>

void windowFunction(std::vector<float>& lBuffer, std::vector<float>& rBuffer);
void magnitudes(std::vector<float>& lBuffer, std::vector<float>& rBuffer);
void equalise(std::vector<float>& lBuffer, std::vector<float>& rBuffer);
void smooth(std::vector<float>& lBuffer, std::vector<float>& rBuffer, int outputSize, float smoothingLevel);
#endif

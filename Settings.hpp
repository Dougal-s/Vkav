#include <unordered_map>
#include <string>

std::unordered_map<std::string, std::string> readConfigFile(const char* filePath);
std::unordered_map<char, const char*> readCmdLineArgs(int argc, char* argv[]);

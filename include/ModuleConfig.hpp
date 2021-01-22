#include <iosfwd>
#include <optional>
#include <string>
#include <variant>
#include <vector>

struct ModuleConfig {
	struct Parameter {
		uint32_t id;
		std::variant<uint32_t, int32_t, float> value;
	};

	struct Resource {
		uint32_t id;
		std::string path;
	};

	std::optional<std::string> moduleName;
	std::optional<uint32_t> vertexCount;

	std::vector<Parameter> params;

	std::vector<Resource> images;
};

ModuleConfig parseConfig(std::istream& stream);

#include <iosfwd>
#include <optional>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

class ParseException : private std::runtime_error {
	uint32_t m_line;
public:
	ParseException(const std::string& what_arg, uint32_t line) : std::runtime_error(what_arg), m_line{line} {}
	ParseException(const char* what_arg, uint32_t line) : std::runtime_error(what_arg), m_line{line} {}
	ParseException(const ParseException& other) noexcept : std::runtime_error(other), m_line{other.m_line} {};

	ParseException& operator=(const ParseException& other) noexcept {
		std::runtime_error::operator=(other);
		m_line = other.m_line;
		return *this;
	}

	virtual const char* what() const noexcept { return std::runtime_error::what(); }

	uint32_t line() const noexcept { return m_line; }
};

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

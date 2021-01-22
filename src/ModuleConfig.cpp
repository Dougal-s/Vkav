#include <cctype>
#include <iomanip>
#include <istream>
#include <limits>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

#include "Calculate.hpp"

#include "ModuleConfig.hpp"

namespace {
	template <char... c>
	std::istream& req(std::istream& s) {
		s >> std::ws;
		if ((... || (s.get() != c))) s.setstate(std::ios::failbit);
		return s;
	}
}  // namespace

ModuleConfig parseConfig(std::istream& stream) {
	ModuleConfig config;

	enum class Section { global, parameters, resources };
	Section section = Section::global;

	while ((stream >> std::ws).good()) {
		// discard comment
		if (stream.peek() == '#') {
			stream.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
			continue;
		}

		// sections
		if (stream.peek() == '[') {
			std::string sectionName;
			if (!std::getline(stream.ignore(), sectionName, ']'))
				throw std::runtime_error("failed to parse section name!");

			if (sectionName == "global")
				section = Section::global;
			else if (sectionName == "parameters")
				section = Section::parameters;
			else if (sectionName == "resources")
				section = Section::resources;
			else
				throw std::runtime_error("unrecognized section name '" + sectionName + "'");
			continue;
		}

		// settings
		if (section == Section::global) {
			std::string name;
			std::string value;

			std::getline(stream, name, '=');
			std::getline(stream >> std::ws, value);

			value = value.substr(0, value.find('#'));

			while (std::isspace(name.back())) name.pop_back();
			while (std::isspace(value.back())) value.pop_back();

			if (name == "module")
				config.moduleName = name;
			else if (name == "vertexCount")
				config.vertexCount = calculate<size_t>(value);
			else
				throw std::runtime_error("unrecognized setting '" + name + "'");
		} else {
			uint32_t id;
			if (!(stream >> req<'('> >> req<'i', 'd'> >> req<'='> >> id >> req<')'>))
				throw std::runtime_error("expected line to start with '(id=ID)'");

			std::string type;
			std::string name;
			std::string valueStr;

			stream >> type >> name >> std::ws;
			if (stream.get() != '=')
				throw std::runtime_error(std::string("expected '=' instead of '") +
				                         static_cast<char>(stream.unget().get()) + "'");

			std::getline(stream >> std::ws, valueStr);
			valueStr = valueStr.substr(0, valueStr.find('#'));

			switch (section) {
				case Section::global:
					break;  // to silence warnings
				case Section::parameters: {
					ModuleConfig::Parameter param = {};
					param.id = id;
					if (type == "int")
						param.value = calculate<int32_t>(valueStr);
					else if (type == "float")
						param.value = calculate<float>(valueStr);
					else
						throw std::runtime_error("Unrecognized parameter type `" + type + "`");

					config.params.push_back(param);
					break;
				}
				case Section::resources: {
					std::string path;
					std::stringstream{valueStr} >> std::quoted(path);

					if (type == "image")
						config.images.push_back({id, path});
					else
						throw std::runtime_error("Unrecognized resource type `" + type + "`");
					break;
				}
			}
		}
	}
	return config;
}

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

	size_t lineNum = 0;
	std::string line_str;
	while (std::getline(stream, line_str)) {
		++lineNum;

		std::stringstream line{std::move(line_str)};
		if ((line >> std::ws).eof()) continue;

		// discard comment
		if (line.peek() == '#') {
			line.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
			continue;
		}

		// sections
		if (line.peek() == '[') {
			std::string sectionName;
			if (!std::getline(line.ignore(), sectionName, ']'))
				throw ParseException("failed to parse section name!", lineNum);

			if (sectionName == "global")
				section = Section::global;
			else if (sectionName == "parameters")
				section = Section::parameters;
			else if (sectionName == "resources")
				section = Section::resources;
			else
				throw ParseException("unrecognized section name '" + sectionName + "'", lineNum);

			if ((line >> std::ws).eof()) continue;
		}

		// settings
		if (section == Section::global) {
			std::string name;
			std::string value;

			std::getline(line, name, '=');
			std::getline(line >> std::ws, value);

			value = value.substr(0, value.find('#'));

			while (std::isspace(name.back())) name.pop_back();
			while (std::isspace(value.back())) value.pop_back();

			if (name == "module")
				config.moduleName = name;
			else if (name == "vertexCount")
				config.vertexCount = calculate<size_t>(value);
			else
				throw ParseException("unrecognized setting '" + name + "'", lineNum);
		} else {
			uint32_t id;
			if (!(line >> req<'('> >> req<'i', 'd'> >> req<'='> >> id >> req<')'>))
				throw ParseException("expected line to start with '(id=ID)'", lineNum);

			std::string type;
			std::string name;
			std::string valueStr;

			line >> type >> name >> std::ws;
			if (line.get() != '=')
				throw ParseException(std::string("expected '=' instead of '") +
				                         static_cast<char>(line.unget().get()) + "'",
				                     lineNum);

			std::getline(line >> std::ws, valueStr);
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
						throw ParseException("Unrecognized parameter type `" + type + "`", lineNum);

					config.params.push_back(param);
					break;
				}
				case Section::resources: {
					std::string path;
					std::stringstream{valueStr} >> std::quoted(path);

					if (type == "image")
						config.images.push_back({id, path});
					else
						throw ParseException("Unrecognized resource type `" + type + "`", lineNum);
					break;
				}
			}
		}
	}
	return config;
}

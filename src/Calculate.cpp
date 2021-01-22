#include <algorithm>
#include <cctype>
#include <cmath>
#include <queue>
#include <stack>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

#include "Calculate.hpp"

#ifdef NDEBUG
#define LOCATION
#else
#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)
#define LOCATION __FILE__ ":" STR(__LINE__) ": "
#endif

namespace {
	struct Token {
		enum class Function { eSin, eCos, eTan, eMax, eMin };
		enum class Type { eUndefined, eNumber, eOperator, eFunction, eParentheses, eComma };

		Type type;
		union {
			float num;
			Function func;
			char op;
		};

		constexpr Token() : type(Type::eUndefined), num(0.f) {}
		constexpr Token(Type tokenType) : type(tokenType), num(0.f) {}
		constexpr Token(Type tokenType, float number) : type(tokenType), num(number) {}
		constexpr Token(Type tokenType, Function function) : type(tokenType), func(function) {}
		constexpr Token(Type tokenType, char operation) : type(tokenType), op(operation) {}
	};

	struct OpProperties {
		unsigned char precedence;
		bool leftAssociative;
	};

	static const std::unordered_map<std::string, Token::Function> functions = {
	    {"sin", Token::Function::eSin},
	    {"cos", Token::Function::eCos},
	    {"tan", Token::Function::eTan},
	    {"max", Token::Function::eMax},
	    {"min", Token::Function::eMin}};

	static const std::unordered_map<char, OpProperties> operators = {
	    {'+', {0, 1}}, {'-', {0, 1}}, {'*', {1, 1}}, {'/', {1, 1}}, {'%', {1, 1}}, {'^', {2, 0}}};

	static const std::unordered_map<std::string, float> constants = {{"e", std::exp(1)},
	                                                                 {"pi", M_PI}};

	static constexpr float eval(char op, float op1, float op2) {
		switch (op) {
			case '+':
				return op1 + op2;
			case '-':
				return op1 - op2;
			case '*':
				return op1 * op2;
			case '/':
				return op1 / op2;
			case '%':
				return std::fmod(op1, op2);
			case '^':
				return std::pow(op1, op2);
		}
		throw std::invalid_argument(LOCATION "Invalid operation!");
	}

	Token extractToken(std::string_view& str, Token::Type lastTokenType) {
		if (str.front() == ',') {
			str.remove_prefix(1);
			return Token(Token::Type::eComma);
		}

		char* ptr;
		if (float value = std::strtof(str.data(), &ptr);
		    lastTokenType != Token::Type::eNumber && ptr != str.data()) {
			str.remove_prefix(ptr - str.data());
			return Token(Token::Type::eNumber, value);
		}

		auto constEnd =
		    std::find_if_not(str.begin(), str.end(), [](char c) { return std::isalpha(c); });
		if (auto it = constants.find(std::string(str.begin(), constEnd)); it != constants.end()) {
			Token rtrn(Token::Type::eNumber, it->second);
			str.remove_prefix(constEnd - str.data());
			return rtrn;
		}

		auto fnSize = str.find('(');
		if (auto it = functions.find(std::string(str, 0, fnSize)); it != functions.end()) {
			Token rtrn(Token::Type::eFunction, it->second);
			str.remove_prefix(fnSize);
			return rtrn;
		}

		if (operators.find(str.front()) != operators.end()) {
			Token rtrn(Token::Type::eOperator, str.front());
			str.remove_prefix(1);
			return rtrn;
		}

		if (str.front() == '(' || str.front() == ')') {
			Token rtrn(Token::Type::eParentheses, str.front());
			str.remove_prefix(1);
			return rtrn;
		}

		throw std::invalid_argument(LOCATION "Unrecognized token!");
	}

	std::queue<Token> constructStack(std::string_view expr) {
		while (std::isspace(expr.back())) expr.remove_suffix(1);

		std::stack<Token> operatorStack;
		std::queue<Token> output;
		Token token;
		while (!expr.empty()) {
			while (std::isspace(expr.front())) expr.remove_prefix(1);
			token = extractToken(expr, token.type);

			switch (token.type) {
				case Token::Type::eNumber:
					output.push(token);
					break;
				case Token::Type::eOperator:
					while (!operatorStack.empty() &&
					       (operatorStack.top().type == Token::Type::eFunction ||
					        (operatorStack.top().type == Token::Type::eOperator &&
					         (operators.find(operatorStack.top().op)->second.precedence +
					              operators.find(operatorStack.top().op)->second.leftAssociative >
					          operators.find(token.op)->second.precedence)))) {
						output.push(operatorStack.top());
						operatorStack.pop();
					}
					[[fallthrough]];
				case Token::Type::eFunction:
					operatorStack.push(token);
					break;
				case Token::Type::eComma:
					while (!operatorStack.empty() &&
					       operatorStack.top().type != Token::Type::eParentheses) {
						output.push(operatorStack.top());
						operatorStack.pop();
					}
					break;
				case Token::Type::eParentheses:
					if (token.op == '(') {
						operatorStack.push(token);
					} else {
						while (!operatorStack.empty() &&
						       operatorStack.top().type != Token::Type::eParentheses) {
							output.push(operatorStack.top());
							operatorStack.pop();
						}
						if (operatorStack.empty())
							throw std::invalid_argument(LOCATION "Mismatched parentheses!");
						operatorStack.pop();
					}
					break;
				default:
					break;
			}
		}
		while (!operatorStack.empty()) {
			output.push(operatorStack.top());
			operatorStack.pop();
		}
		return output;
	}

	float deconstructStack(std::queue<Token>& tokens) {
		std::stack<float> operandStack;
		while (!tokens.empty()) {
			Token token = tokens.front();
			tokens.pop();
			switch (token.type) {
				case Token::Type::eNumber:
					operandStack.push(token.num);
					break;
				case Token::Type::eOperator: {
					if (operandStack.size() < 2)
						throw std::invalid_argument(
						    std::string(LOCATION "Encountered unexpected operator '") + token.op +
						    "'");

					float op1 = operandStack.top();
					operandStack.pop();
					float op2 = operandStack.top();
					operandStack.pop();
					operandStack.push(eval(token.op, op2, op1));
				} break;
				case Token::Type::eFunction: {
					switch (token.func) {
						case Token::Function::eSin:
							if (operandStack.empty())
								throw std::invalid_argument(LOCATION "Expected function argument");
							operandStack.top() = std::sin(operandStack.top());
							break;
						case Token::Function::eCos:
							if (operandStack.empty())
								throw std::invalid_argument(LOCATION "Expected function argument");
							operandStack.top() = std::cos(operandStack.top());
							break;
						case Token::Function::eTan:
							if (operandStack.empty())
								throw std::invalid_argument(LOCATION "Expected function argument");
							operandStack.top() = std::tan(operandStack.top());
							break;
						case Token::Function::eMax: {
							if (operandStack.size() < 2)
								throw std::invalid_argument(LOCATION "Expected function arguments");
							float arg1 = operandStack.top();
							operandStack.pop();
							float arg2 = operandStack.top();
							operandStack.pop();
							operandStack.push(std::max(arg1, arg2));
						} break;
						case Token::Function::eMin: {
							if (operandStack.size() < 2)
								throw std::invalid_argument(LOCATION "Expected function arguments");
							float arg1 = operandStack.top();
							operandStack.pop();
							float arg2 = operandStack.top();
							operandStack.pop();
							operandStack.push(std::min(arg1, arg2));
						} break;
					}
				} break;
				default:
					throw std::invalid_argument(LOCATION "Invalid token!");
			}
		}
		return operandStack.top();
	}
}  // namespace

template <class NumType>
NumType calculate(std::string_view expr) {
	auto tokens = constructStack(expr);
	return static_cast<NumType>(deconstructStack(tokens));
}

template int calculate<int>(std::string_view expr);
template size_t calculate<size_t>(std::string_view expr);
template float calculate<float>(std::string_view expr);

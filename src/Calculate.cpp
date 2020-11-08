#include <algorithm>
#include <cctype>
#include <cmath>
#include <queue>
#include <stack>
#include <unordered_map>
#include <utility>

#include "Calculate.hpp"

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)
#define LOCATION __FILE__ ":" STR(__LINE__) ": "

struct Token {
	enum class Function { eSin, eCos, eTan, eMax, eMin };
	enum class Type { eUndefined, eNumber, eOperator, eFunction, eParentheses, eComma };

	Type type;
	union {
		float num;
		Function func;
		char op;
	};

	Token() : type(Type::eUndefined) {}
	Token(Type tokenType) : type(tokenType) {}
	Token(Type tokenType, float number) : type(tokenType), num(number) {}
	Token(Type tokenType, Function function) : type(tokenType), func(function) {}
	Token(Type tokenType, char operation) : type(tokenType), op(operation) {}
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

static const std::unordered_map<std::string, float> constants = {{"e", std::exp(1)}, {"pi", M_PI}};

float eval(char op, float op1, float op2) {
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

Token extractToken(std::string& expr, Token::Type lastTokenType) {
	if (expr.front() == ',') {
		expr.erase(0, 1);
		return Token(Token::Type::eComma);
	}

	char* ptr;
	if (float value = std::strtof(expr.c_str(), &ptr);
	    lastTokenType != Token::Type::eNumber && ptr != expr.c_str()) {
		expr.erase(0, ptr - expr.c_str());
		return Token(Token::Type::eNumber, value);
	}

	if (auto index = expr.find_first_not_of("abcdefghijklmnopqrstuvwxyz");
	    constants.find(expr.substr(0, index)) != constants.end()) {
		Token rtrn(Token::Type::eNumber, constants.find(expr.substr(0, index))->second);
		expr.erase(0, index);
		return rtrn;
	}

	if (auto index = expr.find('('); functions.find(expr.substr(0, index)) != functions.end()) {
		Token rtrn(Token::Type::eFunction, functions.find(expr.substr(0, index))->second);
		expr.erase(0, index);
		return rtrn;
	}

	if (operators.find(expr.front()) != operators.end()) {
		Token rtrn(Token::Type::eOperator, expr.front());
		expr.erase(0, 1);
		return rtrn;
	}

	if (expr.front() == '(' || expr.front() == ')') {
		Token rtrn(Token::Type::eParentheses, expr.front());
		expr.erase(0, 1);
		return rtrn;
	}

	throw std::invalid_argument(LOCATION "Unrecognized token!");
}

std::queue<Token> constructStack(std::string expr) {
	expr.resize(std::remove_if(expr.begin(), expr.end(), [](char x) { return std::isspace(x); }) -
	            expr.begin());

	std::stack<Token> operatorStack;
	std::queue<Token> output;
	Token token;
	while (!expr.empty()) {
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
				float op1 = operandStack.top();
				operandStack.pop();
				float op2 = operandStack.top();
				operandStack.pop();
				operandStack.push(eval(token.op, op2, op1));
			} break;
			case Token::Type::eFunction: {
				switch (token.func) {
					case Token::Function::eSin:
						operandStack.top() = std::sin(operandStack.top());
						break;
					case Token::Function::eCos:
						operandStack.top() = std::cos(operandStack.top());
						break;
					case Token::Function::eTan:
						operandStack.top() = std::tan(operandStack.top());
						break;
					case Token::Function::eMax: {
						float arg1 = operandStack.top();
						operandStack.pop();
						float arg2 = operandStack.top();
						operandStack.pop();
						operandStack.push(std::max(arg1, arg2));
					} break;
					case Token::Function::eMin: {
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

template <class NumType>
NumType calculate(std::string expr) {
	std::queue<Token> tokens = constructStack(std::move(expr));
	return static_cast<NumType>(deconstructStack(tokens));
}

template int calculate<int>(std::string expr);
template size_t calculate<size_t>(std::string expr);
template float calculate<float>(std::string expr);

#include <algorithm>
#include <cctype>
#include <cmath>
#include <queue>
#include <stack>
#include <unordered_map>
#include <unordered_set>

#include "Calculate.hpp"

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)
#define LOCATION __FILE__ ":" STR(__LINE__) ": "

enum Function { SIN, COS, TAN, MAX, MIN };
enum TokenType { UNDEFINED, NUMBER, FUNCTION, OPERATOR, PARENTHESES };

struct Token {
	TokenType type;
	union {
		float num;
		Function func;
		char op;
	};

	Token() : type(UNDEFINED) {}
	Token(TokenType tokenType, float number) : type(tokenType), num(number) {}
	Token(TokenType tokenType, Function function) : type(tokenType), func(function) {}
	Token(TokenType tokenType, char operation) : type(tokenType), op(operation) {}
};

struct OpProperties {
	unsigned char precedence;
	bool leftAssociative;
};

static const std::unordered_map<std::string, Function> functions = {
    {"sin", SIN}, {"cos", COS}, {"tan", TAN}, {"max", MAX}, {"min", MIN}};

static const std::unordered_map<char, OpProperties> operators = {
    {'+', {0, 1}}, {'-', {0, 1}}, {'*', {1, 1}}, {'/', {1, 1}}, {'%', {1, 1}}, {'^', {2, 0}}};

static const std::unordered_map<std::string, float> constants = {
    {"e", std::exp(1)}, {"pi", 3.1415926535897932384626433}};

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

Token extractToken(std::string& expr, TokenType lastTokenType) {
	size_t index;
	if (expr.front() == ',') expr.erase(0, 1);

	char* ptr;
	if (float value = std::strtof(expr.c_str(), &ptr);
	    lastTokenType != NUMBER && ptr != expr.c_str()) {
		Token rtrn(NUMBER, value);
		expr.erase(0, ptr - expr.c_str());
		return rtrn;
	}

	if (constants.find(expr.substr(
	        0, index = expr.find_first_not_of("abcdefghijklmnopqrstuvwxyz"))) != constants.end()) {
		Token rtrn(NUMBER, constants.find(expr.substr(0, index))->second);
		expr.erase(0, index);
		return rtrn;
	}

	if (functions.find(expr.substr(0, index = expr.find('('))) != functions.end()) {
		Token rtrn(FUNCTION, functions.find(expr.substr(0, index))->second);
		expr.erase(0, index);
		return rtrn;
	}

	if (operators.find(expr.front()) != operators.end()) {
		Token rtrn(OPERATOR, expr.front());
		expr.erase(0, 1);
		return rtrn;
	}

	if (expr.front() == '(' || expr.front() == ')') {
		Token rtrn(PARENTHESES, expr.front());
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
			case NUMBER:
			case FUNCTION:
				output.push(token);
				break;
			case OPERATOR:
				while (!operatorStack.empty() &&
				       (operatorStack.top().type == FUNCTION ||
				        (operatorStack.top().type == OPERATOR &&
				         (operators.find(operatorStack.top().op)->second.precedence +
				              operators.find(operatorStack.top().op)->second.leftAssociative >
				          operators.find(expr.front())->second.precedence)))) {
					output.push(operatorStack.top());
					operatorStack.pop();
				}
				operatorStack.push(token);
				break;
			case PARENTHESES:
				if (token.op == '(') {
					operatorStack.push(token);
				} else {
					while (!operatorStack.empty() && operatorStack.top().type != PARENTHESES) {
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
			case NUMBER:
				operandStack.push(token.num);
				break;
			case OPERATOR: {
				float op1 = operandStack.top();
				operandStack.pop();
				float op2 = operandStack.top();
				operandStack.pop();
				operandStack.push(eval(token.op, op2, op1));
			} break;
			case FUNCTION: {
				switch (token.func) {
					case SIN:
						operandStack.top() = std::sin(operandStack.top());
						break;
					case COS:
						operandStack.top() = std::cos(operandStack.top());
						break;
					case TAN:
						operandStack.top() = std::tan(operandStack.top());
						break;
					case MAX: {
						float arg1 = operandStack.top();
						operandStack.pop();
						float arg2 = operandStack.top();
						operandStack.pop();
						operandStack.push(std::max(arg1, arg2));
					} break;
					case MIN: {
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
NumType calculate(const std::string& expr) {
	std::queue<Token> tokens = constructStack(expr);
	return static_cast<NumType>(deconstructStack(tokens));
}

template int calculate<int>(const std::string& expr);
template size_t calculate<size_t>(const std::string& expr);
template float calculate<float>(const std::string& expr);

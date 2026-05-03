#include "parse.hpp"

#include <algorithm>
#include <cstdlib>

using namespace std;
//page level functions
namespace {
  //error catcher function
[[noreturn]] void failConditionalParse(const string& message) {
  cout << "Error: " << message << endl;
  exit(1);
}
//thing to verify the boolean token
bool isBooleanOperatorToken(const string& token) {
  string normalized = token;
  transform(normalized.begin(), normalized.end(), normalized.begin(), ::tolower);
  return normalized == "and" || normalized == "or" || normalized == "xor";
}
}

//moves the pointer forward to ignore spaces and tabs
void skipWhitespace(const string& str, size_t& pos) {
  while (pos < str.length() && isspace(str[pos])) {
    pos++;
  }
}

// function to return a pseudo random number using xorshift32 
unsigned int xorshift(unsigned int &x) {
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return x;
}

static unsigned int x = time(0); // persistant seed to maintain randomness even in quick repeated generations

int randInRange(int lb, int ub) {

    unsigned int range = ub - lb + 1;
    unsigned int limit = UINT32_MAX - (UINT32_MAX % range);

    unsigned int val;
    do {
        val = xorshift(x);
    } while (val >= limit);

    return lb + (val % range);
}

//reads a word, if digit, converts to float, if letters, pulls the value from variables map
//also handles negative sign and restarts if it sees ( ).
float parseFactor(const string& str, size_t& pos, map<string, varValue>* variables) {
  skipWhitespace(str, pos);
  if (pos >= str.length()) return 0.0f;

  float sign = 1.0f;
  if (str[pos] == '-') {
    sign = -1.0f;
    pos++;
    skipWhitespace(str, pos);
  }
  if(str[pos] == 'R'){ // parsing random variable
    pos += 6; // 'R' 'A' 'N' 'D' 'O' 'M' skips RANDOM
    skipWhitespace(str, pos); // now pos is standing at '('
    if(str[pos] != '('){
      failConditionalParse("Expected '(' in RANDOM");
    }
    pos++;
    int a = parseExpression(str, pos, variables);
    skipWhitespace(str, pos);
    
    if (str[pos] != ',') failConditionalParse("Expected ',' in RANDOM");
    pos++;
    
    int b = parseExpression(str, pos, variables);
    if(a > b){
      failConditionalParse("a expected to be <= b in RANDOM");
    }
    skipWhitespace(str, pos);

    if (str[pos] != ')') failConditionalParse("Expected ')'");
    pos++;

    return sign*randInRange(a, b);
  }
  if (str[pos] == '(') {
    pos++;
    float val = parseExpression(str, pos, variables);
    skipWhitespace(str, pos);
    if (pos < str.length() && str[pos] == ')') pos++;
    return sign * val;
  }

  string token = "";
  while (pos < str.length() && (isalnum(str[pos]) || str[pos] == '_' || str[pos] == '.')) {
    token += str[pos++];
  }

  if (token.empty()) return 0.0f;

  if (isdigit(token[0]) || token[0] == '.') {
    return sign * stof(token);
  } else {
    if (!checkVarExists(token, variables)) return 0.0f;
    return sign * (*variables)[token].f_val;
  }
}
        
//handles multiplication, division, and modulus
//makes sure these happen before addition or subtraction.
float parseTerm(const string& str, size_t& pos, map<string, varValue>* variables) {
  float val = parseFactor(str, pos, variables);
  while (true) {
    skipWhitespace(str, pos);
    if (pos >= str.length()) break;
                
    char op = str[pos];
    if (op != '*' && op != '/' && op != '%') break;
                
    pos++;
    float nextVal = parseFactor(str, pos, variables);
                
    if (op == '*') val *= nextVal;
    else if (op == '/') {
      if (nextVal != 0.0f) val /= nextVal;
      else cout << "Error: Division by zero!" << endl;
    }
    else if (op == '%') {
      // % symbol doesn't work on floats in C++ so fmod is used
      if (nextVal != 0.0f) val = fmod(val, nextVal);
      else cout << "Error: Modulo by zero!" << endl;
    }
  }
  return val;
}
        
//handles addition and subtraction.
//starting funciton of the parser
float parseExpression(const string& str, size_t& pos, map<string, varValue>* variables) {
  float val = parseTerm(str, pos, variables);
  while (true) {
    skipWhitespace(str, pos);
    if (pos >= str.length()) break;
                
    char op = str[pos];
    if (op != '+' && op != '-') break;
                
    pos++;
    float nextVal = parseTerm(str, pos, variables);
                
    if (op == '+') val += nextVal;
    else if (op == '-') val -= nextVal;
  }
  return val;
}
//function to evaluate comparative condition inside if/elif and while statement
bool parseComparisionConditions(const string& conditionalStatement, map<string, varValue>* variables){
  string normalized = trim(conditionalStatement);
  string lowered = normalized;
  transform(lowered.begin(), lowered.end(), lowered.begin(), ::tolower);

  if (lowered == "true") return true;
  if (lowered == "false") return false;

  stringstream ss(normalized);
    string leftSide, op, rightSide;
  if (!(ss >> leftSide >> op >> rightSide)) {
    failConditionalParse("Malformed comparison in IF/ELIF statement: '" + conditionalStatement + "'.");
  }

    float leftVal = getValue(leftSide, variables);
    float rightVal = getValue(rightSide, variables);

    bool conditionMet = false;
    if (op == "==") conditionMet = (leftVal == rightVal);
    else if (op == "!=") conditionMet = (leftVal != rightVal);
    else if (op == "<") conditionMet = (leftVal < rightVal);
    else if (op == ">") conditionMet = (leftVal > rightVal);
    else if (op == "<=") conditionMet = (leftVal <= rightVal);
    else if (op == ">=") conditionMet = (leftVal >= rightVal);
    else {
      failConditionalParse("Unknown operator '" + op + "' in IF/ELIF statement.");
    }
    return conditionMet;
}
//function to evaluate boolean conditions inside if/elif and while statement
bool parseBooleanConditions(stringstream& nestedConditionalStatement, map<string, varValue>* variables) {
    
  vector<string> parts = separateThestringstream(nestedConditionalStatement);
    if (parts.empty()) {
      failConditionalParse("Empty IF/ELIF condition.");
    }

    if (parts.size() % 2 == 0) {
      failConditionalParse("Malformed IF/ELIF condition: missing comparison around boolean operator.");
    }

      for (size_t i = 0; i < parts.size(); i++) {
      if (parts[i].find('(') != string::npos || parts[i].find(')') != string::npos) {
        failConditionalParse("Bracket grouping is not supported in IF/ELIF conditions.");
      }

      if (i % 2 == 0 && isBooleanOperatorToken(parts[i])) {
        failConditionalParse("Malformed IF/ELIF condition near boolean operator '" + parts[i] + "'.");
      }

      if (i % 2 == 1 && !isBooleanOperatorToken(parts[i])) {
        failConditionalParse("Expected boolean operator (and/or/xor) but found '" + parts[i] + "'.");
      }
    }

  vector<string> temp;

  // PASS 1: AND / XOR
  for (size_t i = 0; i < parts.size(); i++) {
      string token = parts[i];

      if (token == "and" || token == "xor") {
        if (temp.empty() || i + 1 >= parts.size()) {
          failConditionalParse("Malformed IF/ELIF condition near operator '" + token + "'.");
        }

          bool left = parseComparisionConditions(temp.back(), variables);
          temp.pop_back();

          bool right = parseComparisionConditions(parts[i + 1], variables);
          i++;

          bool result = (token == "and") ? (left && right) : (left ^ right);

          temp.push_back(result ? "true" : "false");
      } else {
          temp.push_back(token);
      }
  }

  // PASS 2: OR
  bool result = parseComparisionConditions(temp[0], variables);

    for (size_t i = 1; i < temp.size(); i += 2) {
      if (i + 1 >= temp.size()) {
        failConditionalParse("Malformed IF/ELIF condition near 'or'.");
      }

      string op = temp[i];
      bool right = parseComparisionConditions(temp[i + 1], variables);

      if (op == "or") {
          result = result || right;
      }
  }

  return result;
}


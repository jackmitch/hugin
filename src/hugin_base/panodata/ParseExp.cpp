// -*- c-basic-offset: 4 -*-

/** @file ParseExp.cpp
 *
 *  @brief functions to parse expressions from strings
 *
 *  @author T. Modes
 *
 */

/*  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This software is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public
 *  License along with this software. If not, see
 *  <http://www.gnu.org/licenses/>.
 *
 */

// parser using shunting yard algorithm using C++11 features
#include <exception>
#include <stack>
#include <queue>
#include <map>
#include <functional>
#define _USE_MATH_DEFINES
#include <cmath>
#include "ParseExp.h"
#include "hugin_utils/utils.h"

#include <panodata/ImageVariableTranslate.h>

namespace Parser
{

typedef std::map<std::string, double> ConstantMap;

namespace ShuntingYard
{
/** internal exception class for all errors */
class ParseException : public std::runtime_error
{
public:
    explicit ParseException(const char *message) : std::runtime_error(message) {};
};

/** classes for parsed tokens in rpn queue */
namespace RPNTokens
{
/** abstract base class */
class TokenBase
{
public:
    virtual void evaluate(std::stack<double>&) = 0;
    virtual ~TokenBase() {};
};

/** single numeric token on rpn queue */
class NumericToken :public TokenBase
{
public:
    explicit NumericToken(double val) : m_value(val) {};
    void evaluate(std::stack<double>& rpnStack) { rpnStack.push(m_value); };
private:
    double m_value;
};

/** unary operator or function on rpn queue */
class FunctionToken : public TokenBase
{
public:
    explicit FunctionToken(std::function<double(double)> func) : TokenBase(), m_function(func) {};
    void evaluate(std::stack<double>& rpnStack)
    {
        if (rpnStack.empty())
        {
            throw ParseException("Unary operator expects one item on stack.");
        }
        const double val = rpnStack.top();
        rpnStack.pop();
        const double newVal = m_function(val);
        if (!std::isinf(newVal) && !std::isnan(newVal))
        {
            rpnStack.push(newVal);
        }
        else
        {
            throw ParseException("Invalid operation");
        };
    };
private:
    std::function<double(double)> m_function;
};

/** binary operator on rpn stack */
class BinaryToken : public TokenBase
{
public:
    explicit BinaryToken(std::function<double(double, double)> func) : TokenBase(), m_function(func) {};
    void evaluate(std::stack<double>& rpnStack)
    {
        if (rpnStack.size() < 2)
        {
            throw ParseException("BinaryOperator expects 2 items on stack.");
        };
        const double right = rpnStack.top();
        rpnStack.pop();
        const double left = rpnStack.top();
        rpnStack.pop();
        const double newVal = m_function(left, right);
        if (!std::isinf(newVal) && !std::isnan(newVal))
        {
            rpnStack.push(newVal);
        }
        else
        {
            throw ParseException("Invalid operation");
        };
    };
private:
    std::function<double(double, double)> m_function;
};

/** if operator on rpn stack */
class IfToken : public TokenBase
{
public:
    void evaluate(std::stack<double>& rpnStack)
    {
        if (rpnStack.size() < 3)
        {
            throw ParseException("IfOperator expects 3 items on stack.");
        };
        const double elseVal = rpnStack.top();
        rpnStack.pop();
        const double ifVal = rpnStack.top();
        rpnStack.pop();
        const double compareVal = rpnStack.top();
        rpnStack.pop();
        if (fabs(compareVal) > 1e-8)
        {
            rpnStack.push(ifVal);
        }
        else
        {
            rpnStack.push(elseVal);
        };
    };
};
} // namespace RPNTokens

/** classes for operators on shunting yards operator stack */
namespace Operators
{
/** base class for operator on shunting yards operator stack */
class OperatorBase
{
public:
    OperatorBase(int prec, bool rightAssoc = false) : m_precedence(prec), m_rightAssoc(rightAssoc) {};
    virtual ~OperatorBase() {};
    const int GetPrecedence() const { return m_precedence; };
    const bool IsRightAssociative() const { return m_rightAssoc; };
    bool ComparePrecedence(const OperatorBase* other)
    {
        if (IsRightAssociative())
        {
            return GetPrecedence() < other->GetPrecedence();
        }
        else
        {
            return GetPrecedence() <= other->GetPrecedence();
        };
    };
    virtual RPNTokens::TokenBase* GetTokenBase() { return nullptr; };
private:
    int m_precedence;
    bool m_rightAssoc;
};

/** function or unary operator on shunting yards operator stack */
class FunctionOperator : public OperatorBase
{
public:
    FunctionOperator(std::function<double(double)> func, int prec = -2, bool rightAssoc = false) : OperatorBase(prec, rightAssoc), m_function(func) {};
    RPNTokens::TokenBase* GetTokenBase() { return new RPNTokens::FunctionToken(m_function); };
private:
    std::function<double(double)> m_function;
};

/** binary operator on stack on shunting yards operator stack */
class BinaryOperator : public OperatorBase
{
public:
    BinaryOperator(std::function<double(double, double)> func, int prec, bool rightAssoc = false) : OperatorBase(prec, rightAssoc), m_function(func) {};
    RPNTokens::TokenBase* GetTokenBase() { return new RPNTokens::BinaryToken(m_function); };
private:
    std::function<double(double, double)> m_function;
};

/** if operator on operator stack, used only for ':' token */
class IfOperator : public OperatorBase
{
public:
    IfOperator() : OperatorBase(0, true) {};
    RPNTokens::TokenBase* GetTokenBase() { return new RPNTokens::IfToken(); }
};
}; // namespace Operators

/** clear the queue */
void ClearQueue(std::queue<RPNTokens::TokenBase*>& input)
{
    while (!input.empty())
    {
        delete input.front();
        input.pop();
    }
}

/** remove all whitespaces and convert to lowercase */
std::string RemoveWhiteSpaces(const std::string& text)
{
    std::string output;
    output.reserve(text.size());
    for (auto c : text)
    {
        if (!isspace(c))
        {
            output.push_back(tolower(c));
        };
    };
    return output;
};

static std::map<std::string, Operators::OperatorBase*> supportedBinaryOperations;
static Operators::OperatorBase* parenthesesOperator = nullptr;
static Operators::OperatorBase* ifOperator = nullptr;
static Operators::OperatorBase* ifOperatorClose = nullptr;
static std::map<std::string, Operators::FunctionOperator*> supportedFunctions;

/** initialize some internal variables */
void InitParser()
{
    if (supportedBinaryOperations.empty())
    {
        supportedBinaryOperations["||"] = new Operators::BinaryOperator(std::logical_or<double>(), 2);
        supportedBinaryOperations["&&"] = new Operators::BinaryOperator(std::logical_and<double>(), 3);
        supportedBinaryOperations["=="] = new Operators::BinaryOperator(std::equal_to<double>(), 4);
        supportedBinaryOperations["!="] = new Operators::BinaryOperator(std::not_equal_to<double>(), 4);
        supportedBinaryOperations["<"] = new Operators::BinaryOperator(std::less<double>(), 5);
        supportedBinaryOperations["<="] = new Operators::BinaryOperator(std::less_equal<double>(), 5);
        supportedBinaryOperations[">"] = new Operators::BinaryOperator(std::greater<double>(), 5);
        supportedBinaryOperations[">="] = new Operators::BinaryOperator(std::greater_equal<double>(), 5);
        supportedBinaryOperations["+"] = new Operators::BinaryOperator(std::plus<double>(), 6);
        supportedBinaryOperations["-"] = new Operators::BinaryOperator(std::minus<double>(), 6);
        supportedBinaryOperations["*"] = new Operators::BinaryOperator(std::multiplies<double>(), 7);
        supportedBinaryOperations["/"] = new Operators::BinaryOperator(std::divides<double>(), 7);
        supportedBinaryOperations["%"] = new Operators::BinaryOperator((double(*)(double, double))fmod, 7);
        supportedBinaryOperations["^"] = new Operators::BinaryOperator((double(*)(double, double))pow, 8, true);
        supportedBinaryOperations["UNARY_MINUS"] = new Operators::FunctionOperator([](double val)->double {return -1 * val; }, 9, true);
    }
    if (!parenthesesOperator)
    {
        parenthesesOperator = new Operators::OperatorBase(-1);
    };
    if (!ifOperator)
    {
        ifOperator = new Operators::OperatorBase(1);
    }
    if (!ifOperatorClose)
    {
        ifOperatorClose = new Operators::IfOperator();
    }
    if (supportedFunctions.empty())
    {
        supportedFunctions["abs"] = new Operators::FunctionOperator((double(*)(double))abs);
        supportedFunctions["sin"] = new Operators::FunctionOperator((double(*)(double))sin);
        supportedFunctions["cos"] = new Operators::FunctionOperator((double(*)(double))cos);
        supportedFunctions["tan"] = new Operators::FunctionOperator((double(*)(double))tan);
        supportedFunctions["asin"] = new Operators::FunctionOperator((double(*)(double))asin);
        supportedFunctions["acos"] = new Operators::FunctionOperator((double(*)(double))acos);
        supportedFunctions["atan"] = new Operators::FunctionOperator((double(*)(double))atan);
        supportedFunctions["exp"] = new Operators::FunctionOperator((double(*)(double))exp);
        supportedFunctions["log"] = new Operators::FunctionOperator((double(*)(double))log);
        supportedFunctions["ceil"] = new Operators::FunctionOperator((double(*)(double))ceil);
        supportedFunctions["floor"] = new Operators::FunctionOperator((double(*)(double))floor);
        supportedFunctions["sqrt"] = new Operators::FunctionOperator((double(*)(double))sqrt);
        supportedFunctions["deg"] = new Operators::FunctionOperator([](double val)->double { return val * 180.0f / M_PI; });
        supportedFunctions["rad"] = new Operators::FunctionOperator([](double val)->double { return val * M_PI / 180.0f; });
    }
};

/** clean up some internal static variables */
void CleanUpParser()
{
    for (auto it : supportedBinaryOperations)
    {
        delete it.second;
    };
    supportedBinaryOperations.clear();
    for (auto it : supportedFunctions)
    {
        delete it.second;
    };
    supportedFunctions.clear();
    if (parenthesesOperator)
    {
        delete parenthesesOperator;
        parenthesesOperator = nullptr;
    };
    if (ifOperator)
    {
        delete ifOperator;
        ifOperator = nullptr;
    };
    if (ifOperatorClose)
    {
        delete ifOperatorClose;
        ifOperatorClose = nullptr;
    };
};

/** compare the first characters in string with supportedBinaryOperations, return longest match */
std::string FindOperator(const std::string& searchString)
{
    std::string foundOperator;
    for (auto it : supportedBinaryOperations)
    {
        const std::string op(it.first);
        if (searchString.compare(0, op.length(), op) == 0)
        {
            if (op.length() > foundOperator.length())
            {
                foundOperator = op;
            }
        };
    };
    return foundOperator;
};

/** convert expression to RPN *
*  @return true, if expression could be converted to Reverse Polish notation (RPN), otherwise false */
bool ConvertToRPN(const std::string& expression, const ConstantMap& constants, std::queue<RPNTokens::TokenBase*>& rpn)
{
    // initialize some internal variables, don't forget to call CleanUpParser() at the end
    InitParser();
    size_t pos = 0;
    std::stack<Operators::OperatorBase*> operatorStack;
    bool lastTokenWasOperator = true;
    const size_t expressionLength = expression.length();
    while (pos < expressionLength)
    {
        const char currentChar = expression[pos];
        if (lastTokenWasOperator && (currentChar == '+' || currentChar == '-'))
        {
            // special handling of +/- prefix
            if (currentChar == '-')
            {
                operatorStack.push(supportedBinaryOperations["UNARY_MINUS"]);
            };
            pos++;
            lastTokenWasOperator = false;
            continue;
        }
        if (isdigit(currentChar) || currentChar == '.')
        {
            // parse numbers
            size_t index;
            double val;
            try
            {
                val = std::stod(expression.substr(pos), &index);
            }
            catch (std::invalid_argument)
            {
                throw ParseException("Invalid number");
            }
            catch (std::out_of_range)
            {
                throw ParseException("Out of range");
            }
            index += pos;
            rpn.push(new RPNTokens::NumericToken(val));
            pos = index;
            lastTokenWasOperator = false;
            continue;
        }
        if (isalpha(currentChar))
        {
            // parse variable or function names
            const size_t found = expression.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789_", pos);
            std::string subString;
            if (found != std::string::npos)
            {
                subString = expression.substr(pos, found - pos);
                // if next character is '(' we found a function name, otherwise we treat it as variable name
                // all white space have been filtered out before
                if (expression[found] == '(')
                {
                    const auto foundFunction = supportedFunctions.find(subString);
                    if (foundFunction == supportedFunctions.end())
                    {
                        throw ParseException("Unknown function");
                    };
                    operatorStack.push(foundFunction->second);
                    operatorStack.push(parenthesesOperator);
                    pos = found + 1;
                    lastTokenWasOperator = true;
                    continue;
                };
            }
            else
            {
                // no further character in string, it can only be a variable name
                subString = expression.substr(pos);
            };
            const auto foundVar = constants.find(subString);
            if (foundVar == constants.end())
            {
                const std::string s = "Unknown variable: " + subString;
                throw ParseException(s.c_str());
            };
            rpn.push(new RPNTokens::NumericToken(foundVar->second));
            pos = found;
            lastTokenWasOperator = false;
            continue;
        };
        if (currentChar == '(')
        {
            // parse left parenthesis
            operatorStack.push(parenthesesOperator);
            pos++;
            lastTokenWasOperator = true;
            continue;
        };
        if (currentChar == ')')
        {
            // closing parenthesis
            bool matchingParenthesis = false;
            while (!operatorStack.empty())
            {
                const int topPrecedenece = operatorStack.top()->GetPrecedence();
                if (topPrecedenece == -2)
                {
                    throw ParseException("Function without matching parenthesis");
                }
                if (topPrecedenece == -1)
                {
                    // we found a matching parenthesis
                    matchingParenthesis = true;
                    operatorStack.pop();
                    break;
                }
                rpn.push(operatorStack.top()->GetTokenBase());
                operatorStack.pop();
            }
            if (!matchingParenthesis)
            {
                throw ParseException("Mismatched parentheses");
            };
            if (!operatorStack.empty() && operatorStack.top()->GetPrecedence() == -2)
            {
                // we found a function on the operator stack
                rpn.push(operatorStack.top()->GetTokenBase());
                operatorStack.pop();
            };
            pos++;
            lastTokenWasOperator = false;
            continue;
        };
        if (currentChar == '?')
        {
            // we found an start of an ?: expression
            while (!operatorStack.empty() && operatorStack.top()->GetPrecedence() > 1)
            {
                rpn.push(operatorStack.top()->GetTokenBase());
                operatorStack.pop();
            };
            operatorStack.push(ifOperator);
            pos++;
            lastTokenWasOperator = true;
            continue;
        }
        if (currentChar == ':')
        {
            bool matchingIf = false;
            while (!operatorStack.empty())
            {
                if (operatorStack.top()->GetPrecedence() == 1)
                {
                    matchingIf = true;
                    operatorStack.pop();
                    break;
                };
                rpn.push(operatorStack.top()->GetTokenBase());
                operatorStack.pop();
            }
            if (!matchingIf)
            {
                throw ParseException("Mismatched ternary operator ?:");
            };
            operatorStack.push(ifOperatorClose);
            pos++;
            lastTokenWasOperator = true;
            continue;
        };
        // and finally the operators
        const std::string foundOperatorString = FindOperator(expression.substr(pos));
        if (foundOperatorString.empty())
        {
            throw ParseException("Invalid operator or unknown character");
        }
        Operators::OperatorBase* foundOperator = supportedBinaryOperations[foundOperatorString];
        while (!operatorStack.empty() && foundOperator->ComparePrecedence(operatorStack.top()))
        {
            rpn.push(operatorStack.top()->GetTokenBase());
            operatorStack.pop();
        };
        operatorStack.push(foundOperator);
        pos += foundOperatorString.length();
        lastTokenWasOperator = true;
    };
    while (!operatorStack.empty())
    {
        switch (operatorStack.top()->GetPrecedence())
        {
            case -2:
                throw ParseException("Function with unbalanced parenthenses");
                break;
            case -1:
                throw ParseException("Unbalanced left and right parenthenses");
                break;
            case 1:
                throw ParseException("If without else");
                break;
        }
        rpn.push(operatorStack.top()->GetTokenBase());
        operatorStack.pop();
    };
    return true;
};

/** evaluate RPN in input
*  @return true, if input could be evaluated, result has the final value; otherwise false and result is unmodified */
bool EvaluateRPN(std::queue<RPNTokens::TokenBase*>& input, double& result)
{
    std::stack<double> stack;
    try
    {
        while (!input.empty())
        {
            RPNTokens::TokenBase* token = input.front();
            token->evaluate(stack);
            delete token;
            input.pop();
        }
    }
    catch (ParseException)
    {
        ClearQueue(input);
        return false;
    };
    ClearQueue(input);
    if (stack.size() == 1)
    {
        result = stack.top();
        return true;
    }
    return false;
};
}

/** parse complete expression in 2 steps */
bool ParseExpression(const std::string& expression, double& result, const ConstantMap& constants, std::string& error)
{
    std::queue<ShuntingYard::RPNTokens::TokenBase*> rpn;
    try
    {
        // remove all white spaces
        const std::string inputExpression = ShuntingYard::RemoveWhiteSpaces(expression);
        if (inputExpression.empty())
        {
            // if expression is now empty, return false
            return false;
        };
        ConstantMap inputConstants;
        // convert constant names to lower case
        for (auto& c : constants)
        {
            inputConstants[hugin_utils::tolower(c.first)] = c.second;
        }
        // add pi
        inputConstants["pi"] = M_PI;
        // convert expression to reverse polish notation rpn
        if (ShuntingYard::ConvertToRPN(inputExpression, inputConstants, rpn))
        {
            // if successful, evaluate queue
            return ShuntingYard::EvaluateRPN(rpn, result);
        }
        else
        {
            // could not convert to RPN
            ShuntingYard::ClearQueue(rpn);
            return false;
        };
    }
    catch (ShuntingYard::ParseException& exception)
    {
        //something went wrong, delete queue and return false
        ShuntingYard::ClearQueue(rpn);
        error = std::string(exception.what());
        return false;
    };
};

ParseVar::ParseVar() : varname(""), imgNr(-1), expression(""), flag(false)
{
};

bool ParseVarNumber(const std::string&s, Parser::ParseVar& var)
{
    std::size_t pos = s.find_first_of("0123456789");
    if (pos == std::string::npos)
    {
        var.varname = s;
        var.imgNr = -1;
    }
    else
    {
        if (pos == 0)
        {
            return false;
        };
        var.varname = s.substr(0, pos);
        if (!hugin_utils::stringToInt(s.substr(pos, s.length() - pos), var.imgNr))
        {
            return false;
        };
    }
#define image_variable( name, type, default_value ) \
    if (HuginBase::PTOVariableConverterFor##name::checkApplicability(var.varname))\
    {\
        return true;\
    };
#include "panodata/image_variables.h"
#undef image_variable
    return false;
};

// parse the given input if it matches a valid expression
void ParseSingleVar(ParseVarVec& varVec, const std::string& s, std::ostream& errorStream)
{
    // parse following regex ([a-zA-Z]{1,3})(\\d*?)=(.*)
    const std::size_t pos = s.find_first_of("=", 0);
    if (pos != std::string::npos && pos > 0 && pos < s.length() - 1)
    {
        ParseVar var;
        var.expression = hugin_utils::StrTrim(s.substr(pos + 1, s.length() - pos - 1));
        if (var.expression.empty())
        {
            errorStream << "The expression \"" << s << "\" does not contain a result." << std::endl;
        }
        else
        {
            const std::string tempString(s.substr(0, pos));
            if (ParseVarNumber(tempString, var))
            {
                varVec.push_back(var);
            }
            else
            {
                // no image variable from pto syntax
                if (tempString.find_first_of("0123456789") == std::string::npos)
                {
                    // constants should not contain digits
                    var.flag = true;
                    varVec.push_back(var);
                }
                else
                {
                    errorStream << "The expression \"" << tempString << "\" is not a valid image variable or constant." << std::endl;
                };
            };
        };
    }
    else
    {
        errorStream << "The expression \"" << s << "\" is incomplete." << std::endl;
    };
};

//parse complete variables string
void ParseVariableString(ParseVarVec& parseVec, const std::string& input, std::ostream& errorStream, void(*func)(ParseVarVec&, const std::string&, std::ostream&))
{
    // split at ","
    std::vector<std::string> splitResult = hugin_utils::SplitString(input, ",");
    for (auto& s : splitResult)
    {
        (*func)(parseVec, s, errorStream);
    };
};

bool UpdateSingleVar(HuginBase::Panorama& pano, const Parser::ParseVar& parseVar, const Parser::ConstantMap& constants, size_t imgNr, std::ostream& statusStream, std::ostream& errorStream)
{
    const HuginBase::SrcPanoImage& srcImg = pano.getImage(imgNr);
    double val = srcImg.getVar(parseVar.varname);
    Parser::ConstantMap constMap(constants);
    constMap["i"] = 1.0*imgNr;
    constMap["val"] = val;
    constMap["hfov"] = srcImg.getHFOV();
    constMap["width"] = srcImg.getWidth();
    constMap["height"] = srcImg.getHeight();
    statusStream << "Updating variable " << parseVar.varname << imgNr << ": " << val;
    std::string error;
    if (Parser::ParseExpression(parseVar.expression, val, constMap, error))
    {
        statusStream << " -> " << val << std::endl;
        HuginBase::Variable var(parseVar.varname, val);
        pano.updateVariable(imgNr, var);
        return true;
    }
    else
    {
        statusStream << std::endl;
        errorStream << "Could not parse given expression \"" << parseVar.expression << "\" for variable " << parseVar.varname << " on image " << imgNr << "." << std::endl;
        if (!error.empty())
        {
            errorStream << "(Error: " << error << ")" << std::endl;
        };
        return false;
    };
};

bool CalculateConstant(HuginBase::Panorama& pano, const Parser::ParseVar& parseVar, Parser::ConstantMap& constants, std::ostream& statusStream, std::ostream& errorStream)
{
    const HuginBase::SrcPanoImage& srcImg = pano.getImage(0);
    double val = 0;
    Parser::ConstantMap constMap(constants);
    constMap["hfov"] = srcImg.getHFOV();
    constMap["width"] = srcImg.getWidth();
    constMap["height"] = srcImg.getHeight();
    statusStream << "Calculating constant " << parseVar.varname << " = ";
    std::string error;
    if (Parser::ParseExpression(parseVar.expression, val, constMap, error))
    {
        statusStream << val << std::endl;
        constants[parseVar.varname] = val;
        return true;
    }
    else
    {
        statusStream << std::endl;
        errorStream << "Could not parse given expression \"" << parseVar.expression << "\" for constant " << parseVar.varname << "." << std::endl;
        if (!error.empty())
        {
            errorStream << "(Error: " << error << ")" << std::endl;
        };
        return false;
    };
};

void PanoParseExpression(HuginBase::Panorama& pano, const std::string& expression, std::ostream& statusStream, std::ostream& errorStream)
{
    ParseVarVec setVars;
    // set locale to C to correctly parse decimal numbers
    char * old_locale = strdup(setlocale(LC_NUMERIC, NULL));
    setlocale(LC_NUMERIC, "C");
    // filter out comments
    std::vector<std::string> lines = hugin_utils::SplitString(expression, "\n");
    std::string filteredExpression;
    filteredExpression.reserve(expression.length());
    for(auto& l:lines)
    { 
        const std::string l0 = hugin_utils::StrTrim(l);
        if (!l0.empty() && l0[0] != '#')
        {
            filteredExpression.append(l).append(",");
        };
    };
    ParseVariableString(setVars, filteredExpression, errorStream, ParseSingleVar);
    if (!setVars.empty())
    {
        Parser::ConstantMap constants;
        constants["imax"] = pano.getNrOfImages() - 1;
        for (auto& var:setVars)
        {
            if (!var.flag)
            {
                // update image variable
                //skip invalid image numbers
                if (var.imgNr >= (int)pano.getNrOfImages())
                {
                    continue;
                };
                if (var.imgNr < 0)
                {
                    // no img number given, apply to all images
                    HuginBase::UIntSet updatedImgs;
                    for (size_t j = 0; j < pano.getNrOfImages(); j++)
                    {
                        //if we already update the variable in this image via links, skip it
                        if (set_contains(updatedImgs, j))
                        {
                            continue;
                        };
                        // skip following images, if expression could not parsed
                        if (!UpdateSingleVar(pano, var, constants, j, statusStream, errorStream))
                        {
                            break;
                        };
                        updatedImgs.insert(j);
                        if (j == pano.getNrOfImages() - 1)
                        {
                            break;
                        };
                        // now remember linked variables
                        const HuginBase::SrcPanoImage& img1 = pano.getImage(j);
#define image_variable( name, type, default_value ) \
    if (HuginBase::PTOVariableConverterFor##name::checkApplicability(var.varname))\
    {\
        if(img1.name##isLinked())\
        {\
            for(size_t k=j+1; k<pano.getNrOfImages(); k++)\
            {\
                if(img1.name##isLinkedWith(pano.getImage(k)))\
                {\
                    updatedImgs.insert(k);\
                }\
            };\
        };\
    };
#include "panodata/image_variables.h"
#undef image_variable
                    };
                }
                else
                {
                    UpdateSingleVar(pano, var, constants, var.imgNr, statusStream, errorStream);
                };
            }
            else
            {
                // handle constants
                CalculateConstant(pano, var, constants, statusStream, errorStream);
            }
        };
    }
    else
    {
        errorStream << "Expression is empty." << std::endl;
    };
    ShuntingYard::CleanUpParser();
    //reset locale
    setlocale(LC_NUMERIC, old_locale);
    free(old_locale);
};

} //namespace Parser

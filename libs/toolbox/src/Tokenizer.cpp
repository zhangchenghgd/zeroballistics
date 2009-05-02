
#include "Tokenizer.h"

#include "Utils.h"


//------------------------------------------------------------------------------
Tokenizer::Tokenizer(const std::string & input_string, const std::string & separator)
{
    setString(input_string, separator);
}

//------------------------------------------------------------------------------
Tokenizer::Tokenizer(const std::string & input_string, char separator)
{
    std::string sep(" ");
    sep[0] = separator;
    setString(input_string, sep);
}


//------------------------------------------------------------------------------
Tokenizer::~Tokenizer()
{
}

//------------------------------------------------------------------------------
/**
 *  Sets the string to be tokenized. The string is capped at the first
 *  newline character encountered.
 */
void Tokenizer::setString(const std::string & input_string,
                          const std::string & separator)
{
    parse_string_ = input_string;

    // cap at '\n'
    std::size_t newline_pos = parse_string_.find('\n');
    if (newline_pos != std::string::npos)
    {
        parse_string_.resize(newline_pos);
    }

    separator_ = separator;

    eatSeparators();    
}

//------------------------------------------------------------------------------
bool Tokenizer::isEmpty() const
{
    return parse_string_.empty();
}

//------------------------------------------------------------------------------
/**
 *  Returns the next word in the parsestring. Throws an exception if
 *  no more tokens are left. Pre&Postcondition: parse_string_ doesn't
 *  start or stop with a separator or whitespace.
 */
std::string Tokenizer::getNextWord()
{
    if (parse_string_.empty()) throw NoMoreTokensException();

    std::size_t sep_pos = parse_string_.find(separator_);

    std::string ret = parse_string_.substr(0, sep_pos);
    trim(ret);

    if (sep_pos == std::string::npos)
    {
        parse_string_.clear();
    } else
    {
        assert(sep_pos + separator_.length() < parse_string_.size());
        parse_string_ = parse_string_.substr(sep_pos + separator_.length(),
                                             std::string::npos);
    }

    eatSeparators();

    return ret;
}


//------------------------------------------------------------------------------
std::string Tokenizer::getRemainingString()
{
    return parse_string_;
}

//------------------------------------------------------------------------------
/**
 *  Eats leading & trailing separators and whitespaces.
 */
void Tokenizer::eatSeparators()
{
    trim(parse_string_);

    while (parse_string_.find(separator_) == 0)
    {
        parse_string_ = parse_string_.substr(separator_.size());
        trim(parse_string_);
    }
    
    while (parse_string_.size() >= separator_.size() &&
           parse_string_.rfind(separator_) == parse_string_.size() - separator_.size())
    {
        parse_string_ = parse_string_.substr(0, parse_string_.size() - separator_.size());
        trim(parse_string_);
    }
}

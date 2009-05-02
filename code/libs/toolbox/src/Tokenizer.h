
#ifndef LIB_TOKENIZER_INCLUDED
#define LIB_TOKENIZER_INCLUDED

#include <string.h>

#include "Exception.h"



//------------------------------------------------------------------------------
class NoMoreTokensException : public Exception
{
 public:
    NoMoreTokensException() : Exception("NoMoreTokensException"){}
};



//------------------------------------------------------------------------------
class Tokenizer
{
 public:
    Tokenizer(const std::string & input_string, const std::string & separator);
    Tokenizer(const std::string & input_string, char separator = ' ');
    virtual ~Tokenizer();

    void setString(const std::string & input_string, const std::string & separator = " ");

    bool isEmpty() const;
    std::string getNextWord();
    std::string getRemainingString();
 private:

    void eatSeparators();
    
    std::string parse_string_;
    std::string separator_;
};


#endif // #ifndef LIB_TOKENIZER_INCLUDED

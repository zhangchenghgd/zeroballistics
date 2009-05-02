
#ifndef BLUEBEARD_REGEX_INCLUDED
#define BLUEBEARD_REGEX_INCLUDED


#include <string>


struct real_pcre;                 /* declaration; the definition is private  */
typedef struct real_pcre pcre;

//------------------------------------------------------------------------------
/**
 *  Validates a string against a given regular expression
 */
class RegEx
{
 public:
    RegEx(const std::string & regular_expression);
    ~RegEx();

    bool match(const std::string & validation_string);

 private:
    pcre * regex_;

};


#endif


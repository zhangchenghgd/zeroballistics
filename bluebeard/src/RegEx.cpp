

#include "RegEx.h"

#include <cstring>
#include <pcre.h>


#include "Exception.h"


//------------------------------------------------------------------------------
/**
 *  validates a string against a given regular expression
 */
RegEx::RegEx(const std::string & regular_expression)
{

    // try to compile this new regex string
    const char * prce_error;
    int pcre_erroff;
    regex_ = pcre_compile(regular_expression.c_str(), PCRE_UTF8, &prce_error, &pcre_erroff, 0);

    // handle failure
    if (regex_ == NULL)
    {
        Exception e("RegEx regular expression: " + regular_expression + " has the following error: " + prce_error);
        throw e;
    }

}

//------------------------------------------------------------------------------
RegEx::~RegEx()
{
    if (regex_ != NULL)
    {
        pcre_free(regex_);
        regex_ = NULL;
    }
}

//------------------------------------------------------------------------------
bool RegEx::match(const std::string & validation_string)
{

    const char* utf8str = validation_string.c_str();
    int	match[3];
    int len = static_cast<int>(strlen(utf8str));
    int result = pcre_exec(regex_, 0, utf8str, len, 0, 0, match, 3);

    if (result >= 0)
    {
        // this ensures that any regex match is for the entire string
        return (match[1] - match[0] == len);
    }
    // invalid string if there's no match or if string or regex is NULL.
    else if ((result == PCRE_ERROR_NOMATCH) || (result == PCRE_ERROR_NULL))
    {
        return false;
    }
    // anything else is an error.
    else
    {
        throw Exception("An internal error occurred while attempting to match an invalid RegEx.");
    }

}

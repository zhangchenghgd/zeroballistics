
#ifndef TOOLBOX_TEXT_VALUE_INCLUDED
#define TOOLBOX_TEXT_VALUE_INCLUDED

#include <iostream>
#include <string>
#include <vector>

#include "Tokenizer.h"
#include "Utils.h"
#include "Exception.h"


//------------------------------------------------------------------------------
class WrongNumberOfElementsException : public Exception
{
 public:
    WrongNumberOfElementsException() : Exception("WrongNumberOfElementsException"){}
};


//------------------------------------------------------------------------------
/**
 *  The interface for a named value that can be read from & written to
 *  a text stream.
 */
class TextValue
{
 public:
    TextValue(const std::string & name) : name_(name) {}
    virtual ~TextValue() {}

    virtual TextValue * clone() const = 0;
    
    virtual std::istream & readFromStream(std::istream & in)  = 0;
    virtual std::ostream & writeToStream (std::ostream & out) const = 0;

    virtual void * getId() = 0;

    const std::string & getName() const
        {
            return name_;
        }
    
 protected:
    std::string name_;
};

//------------------------------------------------------------------------------
/**
 *  Implements the TextValue interface. Keeps a pointer to a value of
 *  arbitrary type supporting operator<< and operator>>.
 */
template <class T>
class TextValuePointer : public TextValue
{
 public:
    TextValuePointer(const std::string & name, T * value) : TextValue(name), value_(value) {}
    virtual ~TextValuePointer() {}

    virtual TextValuePointer<T> * clone() const
    {
        return new TextValuePointer<T>(name_, value_);
    }
    
    virtual inline std::istream & readFromStream(std::istream & in)
    {
        try
        {
            in >> *value_;

            if (!in) throw Exception("Cannot parse input");
        } catch (WrongNumberOfElementsException & ne)
        {
            ne << "Wrong number of array elements specified for " << name_ << ".\n";
            throw;
        }
        return in;
    }
    
    virtual inline std::ostream & writeToStream (std::ostream & out) const
    {
        out << *value_;
        return out;
    }

    virtual void * getId()
    {
        return value_;
    }
    
 private:
    T * value_;
};

//------------------------------------------------------------------------------
/**
 *  Specialization for strings: we want to read entire string, not
 *  just until the next space.
 */
template <>
inline std::istream & TextValuePointer<std::string>::readFromStream(std::istream & in)
{
    *value_ = "";
    
    std::string sub_str;
    while (in >> sub_str)
    {
        if (!value_->empty()) *value_ += ' ';
        *value_ += sub_str;
    }

    return in;
}


//------------------------------------------------------------------------------
/**
 *  Output a pointer type as number (eg for debugging)
 */
/* XXXX
template<class T>
std::istream & operator>>(std::istream & in, T* &p)
{
    in >> p;
    
    return in;
}
*/

//------------------------------------------------------------------------------
/**
 *  Read an array of Ts from a stream. The array must be of the format
 *  [a;b;c]. The input operator >> must be defined for Ts. There must
 *  be at least as many element as array's size or a
 *  WrongNumberOfElementsException is thrown.
 */
template<class T>
std::istream & operator>>(std::istream & in, std::vector<T> & array)
{
    std::string str;
    std::getline(in, str, '\n');

    trim(str); // get rid of leading & trailing spaces

    if (str.length() <2) throw Exception("Bad format in array");
    // here we drop the leading and trailing brackets
    Tokenizer tokenizer(str.substr(1, str.length()-2), ';');
            
    std::string cur_string;
    std::string token;

    unsigned size = array.size();
    unsigned element_number = 0;
    unsigned num_open_brackets = 0;
    while (!tokenizer.isEmpty())
    {
        token = tokenizer.getNextWord();
        trim(token);
        
        cur_string += token;

        // if there is a '[' in the token, we have entered a
        // subarray. Replace the consumed ';' character.
        if (token.find('[') != std::string::npos)
        {
            ++num_open_brackets;
            if (token.find(']') == std::string::npos)
            {
                cur_string += ';';
                continue;
            }
        }

        if (token.find(']') != std::string::npos)
        {
            if(--num_open_brackets != 0) cur_string += ';';
        } else if (num_open_brackets != 0)
        {
            cur_string += ';';
        }
        if (num_open_brackets != 0) continue;
        
        std::istringstream str_stream(cur_string);
        T array_element;
        if (! (str_stream >> array_element)) break;
        
        if (element_number >= size)
        {
            array.push_back(array_element);
        } else
        {
            array[element_number] = array_element;
        }

        ++element_number;
        cur_string = "";
    }

    if (element_number < size)
    {
        throw WrongNumberOfElementsException();
    }
    
    return in;
}

//------------------------------------------------------------------------------
template<class T>
std::ostream & operator<<(std::ostream & out, const std::vector<T> & array)
{
    out << "[";

    for (unsigned el=0; el < array.size(); ++el)
    {
        if (el != 0) out << ";";
        out << array[el];
    }

    out << "]";

    return out;
}



#endif // #ifndef STUNTS_TEXT_VALUE_INCLUDED

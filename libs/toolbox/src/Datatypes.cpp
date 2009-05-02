
#include "Datatypes.h"

#include <limits>

#include "TextValue.h"
#include "Serializer.h"

#undef min
#undef max

//------------------------------------------------------------------------------
Color & Color::operator+=(const Color & c)
{
    r_ += c.r_;
    g_ += c.g_;
    b_ += c.b_;
    a_ += c.a_;

    return *this;
}

//------------------------------------------------------------------------------
Color & Color::operator*=(float f)
{
    r_ *= f;
    g_ *= f;
    b_ *= f;

    return *this;
}

//------------------------------------------------------------------------------
Color Color::operator*(float f) const
{
    Color ret = *this;
    ret *= f;
    return ret;
}

//------------------------------------------------------------------------------
float Color::getBrightness() const
{
    return std::max(std::max(r_,g_), b_);
}



//------------------------------------------------------------------------------
std::istream & operator>>(std::istream & in, Color & color)
{
    std::vector<float> values;
    in >> values;
    if (values.size() != 3 && values.size() != 4)
    {
        color = Color(1.0f, 0.0f, 1.0f);
        return in;
    }

    color.r_ = values[0];
    color.g_ = values[1];
    color.b_ = values[2];
    color.a_ = values.size() == 4 ? values[3] : 1.0f;

    return in;
}

//------------------------------------------------------------------------------
std::ostream & operator<<(std::ostream & out, const Color & color)
{
    out << std::setw(6);
    out << "[" << color.r_
        << ";" << color.g_
        << ";" << color.b_;

    if (color.a_ != 1.0f) out << ";" << color.a_;
    
    out << "]";
    
    return out;
}

//------------------------------------------------------------------------------
std::istream & operator>>(std::istream & in, TexCoord & t)
{
    std::vector<float> values;
    in >> values;
    if (values.size() != 2)
    {
        t = TexCoord(0.0f, 0.0f);
        return in;
    }

    t.tu_ = values[0];
    t.tv_ = values[1];

    return in;
}


//------------------------------------------------------------------------------
std::ostream & operator<<(std::ostream & out, const TexCoord & t)
{
    out << std::setw(6);
    out << "[" << t.tu_
        << ";" << t.tv_
        << "]";
        
    return out;
}


namespace serializer
{
    
//------------------------------------------------------------------------------
void putInto(Serializer & s, const Color & c)
{
    s.put(c.r_);
    s.put(c.g_);
    s.put(c.b_);
    s.put(c.a_);
}
    
//------------------------------------------------------------------------------
void getFrom(Serializer & s, Color & c)
{
    s.get(c.r_);
    s.get(c.g_);
    s.get(c.b_);
    s.get(c.a_);
}


//------------------------------------------------------------------------------
void putInto(Serializer & s, const Material & m)
{
    s.put(m.ambient_);
    s.put(m.diffuse_);
    s.put(m.specular_);
    s.put(m.shininess_);
}


//------------------------------------------------------------------------------
void getFrom(Serializer & s, Material & m)
{
    s.get(m.ambient_);
    s.get(m.diffuse_);
    s.get(m.specular_);
    s.get(m.shininess_);
}


//------------------------------------------------------------------------------
void putInto(Serializer & s, const TexCoord & t)
{
    s.put(t.tu_);
    s.put(t.tv_);
}

//------------------------------------------------------------------------------
void getFrom(Serializer & s, TexCoord & t)
{
    s.get(t.tu_);
    s.get(t.tv_);
}


} //namespace serializer

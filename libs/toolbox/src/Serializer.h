/*******************************************************************************
 *
 *  Copyright 2004 Muschick Christian
 *  
 *  This file is part of Lear.
 *  
 *  Lear is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *  
 *  Lear is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *  
 *  You should have received a copy of the GNU General Public License
 *  along with Lear; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 * 
 *  -----------------------------------------------------------------------------
 *
 *  filename            : Serializer.h
 *  author              : Muschick Christian
 *  date of creation    : 14.11.2003
 *  date of last change : 01.12.2004
 *
 *
 *******************************************************************************/

#ifdef _WIN32
#pragma warning (disable : 4290) //"C++ exception specification ignored except to indicate a function is not __declspec(nothrow)"
#endif


#ifndef STUNTS_SERIALIZER_INCLUDED
#define STUNTS_SERIALIZER_INCLUDED

#include <fstream>
#include <vector>
#include <map>
#include <stack>

#include "Exception.h"
#include "Datatypes.h"
#include "Utils.h"

#ifdef _WIN32
#define ZLIB_DLL
#endif


#ifndef NO_ZLIB
#include "gzstream.h"
#endif

namespace serializer
{

//------------------------------------------------------------------------------
/**
 *  If tagging is enabled, every fundamental datatype written to disk
 *  is preceded by a marker tag corresponding to the type. This way
 *  inconsistencies in the reading / writing code are easily detected.
 */
enum SERIALIZER_TAG
{
    ST_UINT8     = 0x10810811,
    ST_UINT16    = 0x11611611,
    ST_INT32     = 0x13213211,
    ST_UINT32    = 0xe132e132,
    ST_FLOAT32   = 0xF32F32FF,
    ST_STRING    = 0xAAAAAAAA,
    ST_OBJECT_ID = 0xcccccccc
};
    
//------------------------------------------------------------------------------
enum SERIALIZER_OPEN_MODE
{
    SOM_READ     = 1,
    SOM_WRITE    = 2,
    SOM_TAG      = 4,
    SOM_COMPRESS = 8,
    SOM_APPEND   = 16
};


/// Magic constant. If this pattern is the first DWORD in the file, it
/// is treated as "tagged".
const unsigned TAGGED_MARKER = 0xFA0DAFCE;

class ISerializable;

typedef unsigned IdType;
const IdType NULL_PTR_ID = ~IdType(0);

typedef std::map<IdType, ISerializable*> IdMap;

//------------------------------------------------------------------------------
class IoException : public Exception
{
 public:
    IoException(std::string msg) : Exception(msg) {}
};

//------------------------------------------------------------------------------
/**
 *  This exception is thrown when a deserialized object contains a
 *  reference to an object which is unknown at the time of the call to
 *  resolveReference().
 */
class UnknownIdException : public Exception
{
 public:
    UnknownIdException() : Exception("IO error: unknown object reference encountered!"){}
};

//------------------------------------------------------------------------------
/**
 *  This exception is thrown when a read tag doesn't match the
 *  expected tag.
 */
class InvalidTagException : public Exception
{
 public:
    InvalidTagException(SERIALIZER_TAG expected, SERIALIZER_TAG read)
        {
            *this << "Invalid tag encountered. Expected " << (void*)expected
                  << ", read " << (void*)read;
        }
};


//------------------------------------------------------------------------------
class IReference
{
 public:
    IReference() {}
    virtual ~IReference() {}

//------------------------------------------------------------------------------
/**
 *  Replaces the stored id of a referenced object with its actual
 *  position in memory.
 *
 *  \param id_map Stores for every retrieved object its id and
 *  location in memory.
 */
    virtual void resolve(IdMap & id_map)     const = 0;
};

//------------------------------------------------------------------------------
/**
 *  We need to know the exact type of the reference so we can do a
 *  dynamic_cast to the right type upon resolving the reference.
 */
template<typename T>
class Reference : public IReference
{
 public:
    Reference(T ** ref, IdType id) :
        reference_(ref), id_(id) {}
    virtual ~Reference() {}
    
//------------------------------------------------------------------------------
    /**
     *  Copies the pointer to the deserialized object with the stored
     *  id into *reference_.
     *
     *  \param id_map A mapping from id to actual address of all
     *  ISerializable objects encountered during deserialization.
     */
    virtual void resolve(IdMap & id_map) const throw(UnknownIdException) 
        {            
            if (id_ == NULL_PTR_ID || id_ == 0)
            {
                *reference_ = NULL;
            } else
            {
                IdMap::const_iterator it = id_map.find(id_);
                if (it == id_map.end())
                {
                    throw UnknownIdException();
                }

                *reference_ = dynamic_cast<T*>(it->second);
            }
        }
    
 protected:
    T ** reference_; ///< A pointer to a pointer that will be set to
                     ///the loaded object.
    IdType id_;      ///< The id of the stored object.
};

//------------------------------------------------------------------------------
/**
 *  This class provides the functionality to easily read/write
 *  fundamental and user-defined datatypes to/from disk, either
 *  compressed or uncompressed. It also provides a way to
 *  transparently serialize objects referencing each other.
 *
 *  To define serialization functionality for a simple datatype, the
 *  functions serializer::getFrom and serializer::putInto must be
 *  defined for this datatype.
 *
 *  The getFrom(Serializer&, ...) and putInto(Serializer&, ...)
 *  functions are NOT meant to be called directly! Use Serializer::put
 *  and Serializer::get instead.
 *
 *  To transparently serialize object references, all objects
 *  referencing and being referenced must derive from the
 *  ISerializable class.
 */
class Serializer
{
 public:
    Serializer();
    Serializer(const std::string & filename, uint32_t mode);
    virtual ~Serializer();

    void open(const std::string & filename, uint32_t mode);
    
    bool isTaggingEnabled() const;

    void close();

//------------------------------------------------------------------------------
/**
 *  Puts a value into the file. The value must either be a fundamental
 *  datatype or the corresponding putInto -function must be defined.
 */
    template<typename T> void put(const T & val)
        {
            putInto(*this, val);
        }
//------------------------------------------------------------------------------
/**
 *  Gets a value from a file. The value must either be a fundamental
 *  datatype or the corresponding getFrom -function must be defined.
 */
    template<typename T> void get(T & val)
        {
            getFrom(*this, val);
        }
    
    void putObjectId(const ISerializable * object);
    void put(bool      val);
    void put(uint8_t   val);
    void put(uint16_t  val);
    void put(int32_t   val);
    void put(uint32_t  val);
    void put(float32_t val);
    void put(const std::string & str);
    void putRaw(const void * data, size_t length);


//------------------------------------------------------------------------------
/**
  *  Reads a stored object id.
  *
  *  \see ISerializable
  *  \see IReference::resolve
  */
    template<typename T> void getObjectId(T ** reference)
        {
            checkTag(ST_OBJECT_ID);
            IdType id;
            readNumber(id);
            
            Reference<T> * new_reference = new Reference<T>(reference, id);
            references_to_resolve_.push_back(new_reference);
        }

    void get(bool      & val);
    void get(uint8_t   & val);
    void get(uint16_t  & val);
    void get(int32_t   & val);
    void get(uint32_t  & val);
    void get(float32_t & val);
    void get(std::string & str);
    void getRaw(void * data, size_t length);


    void addId(IdType id, ISerializable * location);
    void addDeserializedObject(ISerializable * object);
    void finalizeDeserialization();
    
private:
    void reset();

    
    void writeBuffer(const void * val, size_t length);
    void readBuffer(       void * val, size_t length); 

    void writeTag(const SERIALIZER_TAG tag);
    void checkTag(const SERIALIZER_TAG tag) throw(InvalidTagException, IoException);
    
    template<typename T> void writeNumber(const T val)
        {
            writeBuffer(&val, sizeof(val));
        }
    
    template<typename T> void readNumber(T & val)
        {
            readBuffer(&val, sizeof(val));
        }

    bool8_t tagging_enabled_;

    IdMap id_map_;

    
    std::vector<IReference*> references_to_resolve_;

    /// All objects derived from ISerializable encountered during
    /// deserialization are stored here so their respective
    /// finalizeDeserialization-methods can be called after all
    /// references have been resolved.
    std::vector<ISerializable*> deserialized_objects_;
    
    std::string filename_;
    
    std::istream * in_;
    std::ostream * out_;
};



//------------------------------------------------------------------------------
/**
 *  Writes a vector to a file. The contained datatype also must have a
 *  serializer function.
 *
 *  \param s The initialized Serializer object to write the vector to.
 *  \param v The vector to be written.
 */
template<typename T>
void putInto(Serializer & s, const std::vector<T> & v)
{
    s.put((uint32_t)v.size());

    for (unsigned i=0; i<v.size(); ++i)
    {
        s.put(v[i]);
    }
}

//------------------------------------------------------------------------------
/**
 *  Reads a vector from disk.
 */
template<typename T>
void getFrom(Serializer & s, std::vector<T> & v)
{
    v.clear();
    
    uint32_t length;
    s.get(length);

    try
    {
        v.resize(length);
    } catch (std::exception & std_e)
    {
        Exception e(std_e.what());
        e.addHistory("getFrom(Serializer & s, std::vector<T> & v)");
        throw e;
    }

    for (unsigned i=0; i<v.size(); ++i)
    {
        s.get(v[i]);
    }
}

//------------------------------------------------------------------------------
/**
 *  Provides a way to serialize objects that keep references to other
 *  objects. The alternative for simpler datatypes is to provide
 *  putInto and getFrom methods for the corresponding datatype.
 *
 *  On serialization, the inverted current address of the
 *  ISerializable object is stored as 'id'. All pointers refering to
 *  other ISerializable objects are inverted and serialized (Inversion
 *  is done so ids cannot be mistakenly used as valid pointers and
 *  bugs involving missing calls to resolveReference() are easily
 *  detected).
 *
 *  During deserialization, a mapping from id to the new actual
 *  address is built. The location of deserialized ids is remembered
 *  and those ids are updated in a final step to refer to the actual
 *  object addresses. Finally, finalizeDeserialization is called for
 *  each deserialized object, which can then perform any task for
 *  which it already needs valid deserialized pointers.
 */
class ISerializable
{
    friend void putInto(Serializer & s, const ISerializable & serializable);
    friend void getFrom(Serializer & s, ISerializable & serializable);
    friend class Serializer;
    
 public:
    ISerializable() {}
    virtual ~ISerializable() {}

 protected:
    
    virtual void getFrom(serializer::Serializer & s)       = 0;
    virtual void putInto(serializer::Serializer & s) const = 0;

//------------------------------------------------------------------------------    
/**
 *  This method gets called after all references have been resolved
 *  and are valid.
 */
    virtual void finalizeDeserialization() {}
};


void putInto(Serializer & s, const ISerializable & serializable); 
void getFrom(Serializer & s, ISerializable & serializable); 


} // namespace serializer 
 
#endif // #ifndef STUNTS_SERIALIZER_INCLUDED

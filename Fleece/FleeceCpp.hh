//
// FleeceCpp.hh
//
// Copyright (c) 2017 Couchbase, Inc All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#pragma once
#ifndef _FLEECE_H
#include "Fleece.h"
#endif
#ifdef __OBJC__
#include "Fleece+CoreFoundation.h"
#endif
#include "slice.hh"
#include <string>

namespace fleeceapi {
    class Array;
    class Dict;
    class MutableArray;
    class MutableDict;
    class KeyPath;


    static inline FLString FLStr(const std::string &s) {
        return {s.data(), s.size()};
    }

    static inline std::string asstring(FLString s) {
        return std::string((char*)s.buf, s.size);
    }

    static inline std::string asstring(FLStringResult &&s) {
        auto str = std::string((char*)s.buf, s.size);
        FLSliceResult_Free(s);
        return str;
    }

    static inline bool operator== (FLSlice s1, FLSlice s2) {return FLSlice_Equal(s1, s2);}
    static inline bool operator!= (FLSlice s1, FLSlice s2) {return !(s1 == s2);}

    static inline bool operator== (FLSliceResult sr, FLSlice s) {return (FLSlice)sr == s;}
    static inline bool operator!= (FLSliceResult sr, FLSlice s) {return !(sr ==s);}
    

    class Value {
    public:
        static Value fromData(FLSlice data)          {return Value(FLValue_FromData(data));}
        static Value fromTrustedData(FLSlice data)   {return Value(FLValue_FromTrustedData(data));}
        
        Value()                                         { }
        Value(FLValue v)                                :_val(v) { }
        operator FLValue const ()                       {return _val;}

        inline FLValueType type() const;
        inline bool isInteger() const;
        inline bool isUnsigned() const;
        inline bool isDouble() const;

        inline bool asBool() const;
        inline int64_t asInt() const;
        inline uint64_t asUnsigned() const;
        inline float asFloat() const;
        inline double asDouble() const;
        inline FLString asString() const;
        inline FLSlice asData() const;
        inline Array asArray() const;
        inline Dict asDict() const;

        inline std::string asstring() const             {return ::fleeceapi::asstring(asString());}

        inline FLStringResult toString() const;
        inline FLStringResult toJSON() const;
        inline FLStringResult toJSON5() const;

        inline FLStringResult toJSON(FLSharedKeys sk,
                                     bool json5 =false,
                                     bool canonical =false);

        explicit operator bool() const                  {return _val != nullptr;}
        bool operator! () const                         {return _val == nullptr;}

        Value& operator= (Value v)                      {_val = v._val; return *this;}

        inline Value operator[] (const KeyPath &kp) const;

#ifdef __OBJC__
        inline id asNSObject(FLSharedKeys sharedKeys =nullptr, NSMapTable *sharedStrings =nil) const {
            return FLValue_GetNSObject(_val, sharedKeys, sharedStrings);
        }
#endif

    protected:
        ::FLValue _val {nullptr};
    };


    class valueptr {  // A bit of ugly glue used to make Array/Dict iterator's operator-> work
    public:
        explicit valueptr(Value v)                      :_value(v) { }
        Value* operator-> ()                            {return &_value;}
    private:
        Value _value;
    };


    class KeyPath {
    public:
        KeyPath(FLSlice specifier, FLSharedKeys sk, FLError *error)
        :_path(FLKeyPath_New(specifier, sk, error))
        { }
        ~KeyPath()                                      {FLKeyPath_Free(_path);}
        explicit operator bool() const                  {return _path != nullptr;}

        static Value eval(FLSlice specifier, FLSharedKeys sk, Value root, FLError *error) {
            return FLKeyPath_EvalOnce(specifier, sk, root, error);
        }

    private:
        KeyPath(const KeyPath&) =delete;
        KeyPath& operator=(const KeyPath&) =delete;
        friend class Value;

        FLKeyPath _path;
    };
    
    
    class Array : public Value {
    public:
        Array()                                         :Value() { }
        Array(FLArray a)                                :Value((FLValue)a) { }
        operator FLArray () const                       {return (FLArray)_val;}

        inline uint32_t count() const;
        inline bool empty() const;
        inline Value get(uint32_t index) const;

        inline Value operator[] (int index) const       {return get(index);}
        inline Value operator[] (const KeyPath &kp) const {return Value::operator[](kp);}

        Array& operator= (Array a)                      {_val = a._val; return *this;}
        Value& operator= (Value v)                      =delete;

        inline MutableArray asMutable() const;

        class iterator : private FLArrayIterator {
        public:
            inline iterator(Array);
            inline iterator(const FLArrayIterator &i)   :FLArrayIterator(i) { }
            inline Value value() const;
            inline uint32_t count() const               {return FLArrayIterator_GetCount(this);}
            inline bool next();
            inline valueptr operator -> () const        {return valueptr(value());}
            inline Value operator * () const            {return value();}
            inline explicit operator bool() const       {return (bool)value();}
            inline iterator& operator++ ()              {next(); return *this;}
            inline bool operator!= (const iterator&)    {return value() != nullptr;}
            inline Value operator[] (unsigned n) const  {return FLArrayIterator_GetValueAt(this,n);}
        private:
            iterator() { }
            friend class Array;
        };

        // begin/end are just provided so you can use the C++11 "for (Value v : array)" syntax.
        inline iterator begin() const                   {return iterator(*this);}
        inline iterator end() const                     {return iterator();}
    };


    class Dict : public Value {
    public:
        Dict()                                          :Value() { }
        Dict(FLDict d)                                  :Value((FLValue)d) { }
        operator FLDict () const                        {return (FLDict)_val;}

        inline uint32_t count() const;
        inline bool empty() const;

        inline Value get(FLString key) const;
        inline Value get(FLString key, FLSharedKeys sk) const;

        inline Value get(const char* key) const                   {return get(FLStr(key));}
        inline Value get(const char *key, FLSharedKeys sk) const  {return get(FLStr(key), sk);}

        inline Value operator[] (FLString key) const    {return get(key);}
        inline Value operator[] (const char *key) const {return get(key);}
        inline Value operator[] (const KeyPath &kp) const {return Value::operator[](kp);}

        Dict& operator= (Dict d)                        {_val = d._val; return *this;}
        Value& operator= (Value v)                      =delete;

        inline MutableDict asMutable() const;

        class Key {
        public:
            // Warning: the input string's memory MUST remain valid for as long as the Key is in
            // use! (The Key stores a pointer to the string, but does not copy it.)
            Key(FLSlice string, bool cachePointers =false);
            Key(FLSlice string, FLSharedKeys sharedKeys);
            inline FLString string() const;
            operator FLString() const                   {return string();}
        private:
            FLDictKey _key;
            friend class Dict;
        };

        inline Value get(Key &key) const;
        inline Value operator[] (Key &key) const        {return get(key);}

        class iterator : private FLDictIterator {
        public:
            inline iterator(Dict);
            inline iterator(Dict, FLSharedKeys);
            inline iterator(const FLDictIterator &i)   :FLDictIterator(i) { }
            inline uint32_t count() const               {return FLDictIterator_GetCount(this);}
            inline Value key() const;
            inline FLString keyString() const;
            inline Value value() const;
            inline bool next();

            inline valueptr operator -> () const        {return valueptr(value());}
            inline Value operator * () const            {return value();}
            inline explicit operator bool() const       {return (bool)value();}
            inline iterator& operator++ ()              {next(); return *this;}
            inline bool operator!= (const iterator&) const    {return value() != nullptr;}

#ifdef __OBJC__
            inline NSString* keyAsNSString(NSMapTable *sharedStrings) const
                                    {return FLDictIterator_GetKeyAsNSString(this, sharedStrings);}
#endif
        private:
            iterator() { }
            friend class Dict;
        };

        // begin/end are just provided so you can use the C++11 "for (Value v : dict)" syntax.
        inline iterator begin() const                   {return iterator(*this);}
        inline iterator end() const                     {return iterator();}
    };


    /** A Dict that manages its own storage. */
    class AllocedDict : public Dict, fleece::alloc_slice {
    public:
        AllocedDict()
        { }

        explicit AllocedDict(const fleece::alloc_slice &s)
        :Dict(FLValue_AsDict(FLValue_FromData(s)))
        ,alloc_slice(s)
        { }

        explicit AllocedDict(fleece::slice s)
        :AllocedDict(alloc_slice(s)) { }

        AllocedDict(const AllocedDict &d)
        :Dict(d)
        ,alloc_slice((const alloc_slice&)d)
        { }

        const alloc_slice& data() const                 {return *this;}
        explicit operator bool () const                 {return Dict::operator bool();}
    };


    class MutableArray : public Array {
    public:
        static MutableArray newArray()          {return MutableArray(FLMutableArray_New(), true);}
        static MutableArray newArray(Array a)   {return MutableArray(FLArray_MutableCopy(a), true);}

        MutableArray(FLMutableArray a)          :Array((FLArray)a) {FLMutableArray_Retain(*this);}
        MutableArray(const MutableArray &a)     :Array((FLArray)a) {FLMutableArray_Retain(*this);}
        MutableArray(MutableArray &&a)          :Array((FLArray)a) {a._val = nullptr;}
        ~MutableArray()                         {FLMutableArray_Release(*this);}

        operator FLMutableArray () const        {return (FLMutableArray)_val;}

        MutableArray& operator= (const MutableArray &a) {
            FLMutableArray_Retain(a);
            FLMutableArray_Release(*this);
            _val = a._val;
            return *this;
        }

        MutableArray& operator= (MutableArray &&a) {
            if (a._val != _val) {
                FLMutableArray_Release(*this);
                _val = a._val;
            }
            return *this;
        }

        Array source() const                    {return FLMutableArray_GetSource(*this);}
        bool isChanged() const                  {return FLMutableArray_IsChanged(*this);}

        void remove(uint32_t first, uint32_t n) {FLMutableArray_Remove(*this, first, n);}
        void resize(uint32_t size)              {FLMutableArray_Resize(*this, size);}

        void setNull(uint32_t i)                {FLMutableArray_SetNull(*this, i);}
        void set(uint32_t i, bool v)            {FLMutableArray_SetBool(*this, i, v);}
        void set(uint32_t i, int64_t v)         {FLMutableArray_SetInt(*this, i, v);}
        void set(uint32_t i, uint64_t v)        {FLMutableArray_SetUInt(*this, i, v);}
        void set(uint32_t i, float v)           {FLMutableArray_SetFloat(*this, i, v);}
        void set(uint32_t i, double v)          {FLMutableArray_SetDouble(*this, i, v);}
        void set(uint32_t i, FLString v)        {FLMutableArray_SetString(*this, i, v);}
        void setData(uint32_t i, FLSlice v)     {FLMutableArray_SetData(*this, i, v);}
        void set(uint32_t i, Value v)           {FLMutableArray_SetValue(*this, i, v);}

        void appendNull()                       {FLMutableArray_AppendNull(*this);}
        void append(bool v)                     {FLMutableArray_AppendBool(*this, v);}
        void append(int64_t v)                  {FLMutableArray_AppendInt(*this, v);}
        void append(uint64_t v)                 {FLMutableArray_AppendUInt(*this, v);}
        void append(float v)                    {FLMutableArray_AppendFloat(*this, v);}
        void append(double v)                   {FLMutableArray_AppendDouble(*this, v);}
        void append(FLString v)                 {FLMutableArray_AppendString(*this, v);}
        void appendData(FLSlice v)              {FLMutableArray_AppendData(*this, v);}
        void append(Value v)                    {FLMutableArray_AppendValue(*this, v);}

        inline MutableArray getMutableArray(uint32_t i);
        inline MutableDict getMutableDict(uint32_t i);

    private:
        MutableArray(FLMutableArray a, bool)     :Array((FLArray)a) {}
    };


    class MutableDict : public Dict {
    public:
        static MutableDict newDict()            {return MutableDict(FLMutableDict_New(), true);}
        static MutableDict newDict(Dict d)      {return MutableDict(FLDict_MutableCopy(d), true);}

        MutableDict(FLMutableDict d)            :Dict((FLDict)d) {FLMutableDict_Retain(*this);}
        MutableDict(const MutableDict &d)       :Dict((FLDict)d) {FLMutableDict_Retain(*this);}
        MutableDict(MutableDict &&d)            :Dict((FLDict)d) {d._val = nullptr;}
        ~MutableDict()                          {FLMutableDict_Release(*this);}

        operator FLMutableDict () const         {return (FLMutableDict)_val;}

        MutableDict& operator= (const MutableDict &d) {
            FLMutableDict_Retain(d);
            FLMutableDict_Release(*this);
            _val = d._val;
            return *this;
        }

        MutableDict& operator= (MutableDict &&d) {
            if (d._val != _val) {
                FLMutableDict_Release(*this);
                _val = d._val;
            }
            return *this;
        }

        Dict source() const                     {return FLMutableDict_GetSource(*this);}
        bool isChanged() const                  {return FLMutableDict_IsChanged(*this);}

        void remove(FLString key)               {FLMutableDict_Remove(*this, key);}

        void setNull(FLString k)                {FLMutableDict_SetNull(*this, k);}
        void set(FLString k, bool v)            {FLMutableDict_SetBool(*this, k, v);}
        void set(FLString k, int64_t v)         {FLMutableDict_SetInt(*this, k, v);}
        void set(FLString k, uint64_t v)        {FLMutableDict_SetUInt(*this, k, v);}
        void set(FLString k, float v)           {FLMutableDict_SetFloat(*this, k, v);}
        void set(FLString k, double v)          {FLMutableDict_SetDouble(*this, k, v);}
        void set(FLString k, FLString v)        {FLMutableDict_SetString(*this, k, v);}
        void setData(FLString k, FLSlice v)     {FLMutableDict_SetData(*this, k, v);}
        void set(FLString k, Value v)           {FLMutableDict_SetValue(*this, k, v);}

        inline MutableArray getMutableArray(FLString key);
        inline MutableDict getMutableDict(FLString key);

    private:
        MutableDict(FLMutableDict d, bool)      :Dict((FLDict)d) {}
    };


    class Encoder {
    public:
        Encoder()                                       :_enc(FLEncoder_New()) { }
        explicit Encoder(FLEncoder enc)                 :_enc(enc) { }

        explicit Encoder(FLEncoderFormat format,
                         size_t reserveSize =0,
                         bool uniqueStrings =true)
        :_enc(FLEncoder_NewWithOptions(format, reserveSize, uniqueStrings))
        { }

        void release()                                  {_enc = nullptr;}
        
        ~Encoder()                                      {FLEncoder_Free(_enc);}

        void setSharedKeys(FLSharedKeys sk)             {FLEncoder_SetSharedKeys(_enc, sk);}

        inline void makeDelta(FLSlice base, bool reuseStrings =true, bool externPointers =false);

        static FLSliceResult convertJSON(FLSlice json, FLError *error) {
            return FLData_ConvertJSON(json, error);
        }

        operator ::FLEncoder ()                         {return _enc;}

        inline bool writeNull();
        inline bool writeBool(bool);
        inline bool writeInt(int64_t);
        inline bool writeUInt(uint64_t);
        inline bool writeFloat(float);
        inline bool writeDouble(double);
        inline bool writeString(FLString);
        inline bool writeString(const char *s)          {return writeString(FLStr(s));}
        inline bool writeString(std::string s)          {return writeString(FLStr(s));}
        inline bool writeData(FLSlice);
        inline bool writeValue(Value, FLSharedKeys =nullptr);
        inline bool convertJSON(FLSlice);

        inline bool beginArray(size_t reserveCount =0);
        inline bool endArray();
        inline bool beginDict(size_t reserveCount =0);
        inline bool writeKey(FLString);
        inline bool endDict();

        inline size_t bytesWritten() const;

        inline FLSliceResult finish(FLError* =nullptr);
        inline void reset();

        inline FLError error() const;
        inline const char* errorMessage() const;

        //////// "<<" convenience operators;

        // Note: overriding <<(bool) would be dangerous due to implicit conversion
        Encoder& operator<< (long long i)           {writeInt(i); return *this;}
        Encoder& operator<< (unsigned long long i)  {writeUInt(i); return *this;}
        Encoder& operator<< (long i)                {writeInt(i); return *this;}
        Encoder& operator<< (unsigned long i)       {writeUInt(i); return *this;}
        Encoder& operator<< (int i)                 {writeInt(i); return *this;}
        Encoder& operator<< (unsigned int i)        {writeUInt(i); return *this;}
        Encoder& operator<< (double d)              {writeDouble(d); return *this;}
        Encoder& operator<< (float f)               {writeFloat(f); return *this;}
        Encoder& operator<< (FLString s)            {writeString(s); return *this;}
        Encoder& operator<< (const  char *str)      {writeString(str); return *this;}
        Encoder& operator<< (const std::string &s)  {writeString(s); return *this;}
        Encoder& operator<< (Value v)               {writeValue(v); return *this;}

#ifdef __OBJC__
        bool writeNSObject(id obj)                 {return FLEncoder_WriteNSObject(_enc, obj);}
        Encoder& operator<< (id obj)               {writeNSObject(obj); return *this;}
        NSData* finishAsNSData(NSError **err)      {return FLEncoder_FinishWithNSData(_enc, err);}
#endif

    protected:
        Encoder(const Encoder&) =delete;
        Encoder& operator=(const Encoder&) =delete;

        FLEncoder _enc;
    };

    class JSONEncoder : public Encoder {
    public:
        JSONEncoder()                           :Encoder(kFLEncodeJSON) { }
        inline bool writeRaw(FLSlice raw)       {return FLEncoder_WriteRaw(_enc, raw);}
    };

    class JSON5Encoder : public Encoder {
    public:
        JSON5Encoder()                          :Encoder(kFLEncodeJSON5) { }
    };


    // Use this instead of Encoder if you don't own the FLEncoder
    class SharedEncoder : public Encoder {
    public:
        explicit SharedEncoder(FLEncoder enc)   :Encoder(enc) { }

        ~SharedEncoder() {
            release(); // prevents Encoder from freeing the FLEncoder
        }
    };


    class Delta {
    public:
        static inline fleece::alloc_slice create(Value old, FLSharedKeys oldSK,
                                                 Value nuu, FLSharedKeys nuuSK);
        static inline bool create(Value old, FLSharedKeys oldSK,
                                  Value nuu, FLSharedKeys nuuSK,
                                  Encoder &jsonEncoder);

        static inline fleece::alloc_slice apply(Value old,
                                                FLSharedKeys sk,
                                                fleece::slice jsonDelta,
                                                FLError *error);
        static inline bool apply(Value old,
                                 FLSharedKeys sk,
                                 Value jsonDelta,
                                 Encoder &encoder);
    };


    class ExternResolver {
    public:
        /** Constructs a resolver for a Fleece document in memory. Extern pointers in it will be
            mapped into the `destination` as though the `destination` preceded the `document` in
            memory. */
        ExternResolver(FLSlice document, FLSlice destination)
        :_document(document)
        {
            FLResolver_Begin(document, destination);
        }

        ~ExternResolver() {
            FLResolver_End(_document);
        }

    private:
        ExternResolver(const ExternResolver&) =delete;
        ExternResolver& operator= (const ExternResolver&) =delete;

        const FLSlice _document;
    };


    //////// IMPLEMENTATION GUNK:

    inline FLValueType Value::type() const      {return FLValue_GetType(_val);}
    inline bool Value::isInteger() const        {return FLValue_IsInteger(_val);}
    inline bool Value::isUnsigned() const       {return FLValue_IsUnsigned(_val);}
    inline bool Value::isDouble() const         {return FLValue_IsDouble(_val);}

    inline bool Value::asBool() const           {return FLValue_AsBool(_val);}
    inline int64_t Value::asInt() const         {return FLValue_AsInt(_val);}
    inline uint64_t Value::asUnsigned() const   {return FLValue_AsUnsigned(_val);}
    inline float Value::asFloat() const         {return FLValue_AsFloat(_val);}
    inline double Value::asDouble() const       {return FLValue_AsDouble(_val);}
    inline FLString Value::asString() const     {return FLValue_AsString(_val);}
    inline FLSlice Value::asData() const        {return FLValue_AsData(_val);}
    inline Array Value::asArray() const         {return FLValue_AsArray(_val);}
    inline Dict Value::asDict() const           {return FLValue_AsDict(_val);}

    inline FLStringResult Value::toString() const {return FLValue_ToString(_val);}
    inline FLStringResult Value::toJSON() const {return FLValue_ToJSON(_val);}
    inline FLStringResult Value::toJSON5() const{return FLValue_ToJSON5(_val);}

    inline FLStringResult Value::toJSON(FLSharedKeys sk, bool json5, bool canonical) {
        return FLValue_ToJSONX(_val, sk, json5, canonical);
    }

    inline Value Value::operator[] (const KeyPath &kp) const
                                                {return FLKeyPath_Eval(kp._path, _val);}


    inline uint32_t Array::count() const        {return FLArray_Count(*this);}
    inline bool Array::empty() const            {return FLArray_IsEmpty(*this);}
    inline Value Array::get(uint32_t i) const   {return FLArray_Get(*this, i);}

    inline Array::iterator::iterator(Array a)   {FLArrayIterator_Begin(a, this);}
    inline Value Array::iterator::value() const {return FLArrayIterator_GetValue(this);}
    inline bool Array::iterator::next()         {return FLArrayIterator_Next(this);}

    inline MutableArray Array::asMutable() const
                        {return MutableArray(FLMutableArray_Retain(FLArray_AsMutable(*this)));}

    inline uint32_t Dict::count() const         {return FLDict_Count(*this);}
    inline bool Dict::empty() const             {return FLDict_IsEmpty(*this);}
    inline Value Dict::get(FLSlice key) const   {return FLDict_Get(*this, key);}
    inline Value Dict::get(FLSlice key, FLSharedKeys sk) const {return FLDict_GetSharedKey(*this, key, sk);}
    inline Value Dict::get(Dict::Key &key) const{return FLDict_GetWithKey(*this, &key._key);}

    inline Dict::Key::Key(FLSlice s, bool c)    :_key(FLDictKey_Init(s, c)) { }
    inline Dict::Key::Key(FLSlice s, FLSharedKeys sk) :_key(FLDictKey_InitWithSharedKeys(s, sk)) { }
    inline FLString Dict::Key::string() const   {return FLDictKey_GetString(&_key);}

    inline Dict::iterator::iterator(Dict d)     {FLDictIterator_Begin(d, this);}
    inline Dict::iterator::iterator(Dict d, FLSharedKeys sk)
                                                {FLDictIterator_BeginShared(d, this, sk);}
    inline Value Dict::iterator::key() const    {return FLDictIterator_GetKey(this);}
    inline FLString Dict::iterator::keyString() const {return FLDictIterator_GetKeyString(this);}
    inline Value Dict::iterator::value() const  {return FLDictIterator_GetValue(this);}
    inline bool Dict::iterator::next()          {return FLDictIterator_Next(this);}

    inline MutableDict Dict::asMutable() const
                        {return MutableDict(FLMutableDict_Retain(FLDict_AsMutable(*this)));}

    inline MutableArray MutableArray::getMutableArray(uint32_t i)
                                                {return FLMutableArray_GetMutableArray(*this, i);}
    inline MutableDict MutableArray::getMutableDict(uint32_t i)
                                                {return FLMutableArray_GetMutableDict(*this, i);}
    inline MutableArray MutableDict::getMutableArray(FLString key)
                                                {return FLMutableDict_GetMutableArray(*this, key);}
    inline MutableDict MutableDict::getMutableDict(FLString key)
                                                {return FLMutableDict_GetMutableDict(*this, key);}

    inline void Encoder::makeDelta(FLSlice base, bool reuseStrings, bool externPointers)
                                                {FLEncoder_MakeDelta(_enc, base,
                                                                     reuseStrings, externPointers);}
    inline bool Encoder::writeNull()            {return FLEncoder_WriteNull(_enc);}
    inline bool Encoder::writeBool(bool b)      {return FLEncoder_WriteBool(_enc, b);}
    inline bool Encoder::writeInt(int64_t n)    {return FLEncoder_WriteInt(_enc, n);}
    inline bool Encoder::writeUInt(uint64_t n)  {return FLEncoder_WriteUInt(_enc, n);}
    inline bool Encoder::writeFloat(float n)    {return FLEncoder_WriteFloat(_enc, n);}
    inline bool Encoder::writeDouble(double n)  {return FLEncoder_WriteDouble(_enc, n);}
    inline bool Encoder::writeString(FLString s){return FLEncoder_WriteString(_enc, s);}
    inline bool Encoder::writeData(FLSlice data){return FLEncoder_WriteData(_enc, data);}
    inline bool Encoder::writeValue(Value v, FLSharedKeys sk)
                                        {return FLEncoder_WriteValueWithSharedKeys(_enc, v, sk);}
    inline bool Encoder::convertJSON(FLSlice j) {return FLEncoder_ConvertJSON(_enc, j);}
    inline bool Encoder::beginArray(size_t rsv) {return FLEncoder_BeginArray(_enc, rsv);}
    inline bool Encoder::endArray()             {return FLEncoder_EndArray(_enc);}
    inline bool Encoder::beginDict(size_t rsv)  {return FLEncoder_BeginDict(_enc, rsv);}
    inline bool Encoder::writeKey(FLString key) {return FLEncoder_WriteKey(_enc, key);}
    inline bool Encoder::endDict()              {return FLEncoder_EndDict(_enc);}
    inline size_t Encoder::bytesWritten() const {return FLEncoder_BytesWritten(_enc);}
    inline FLSliceResult Encoder::finish(FLError* err) {return FLEncoder_Finish(_enc, err);}
    inline void Encoder::reset()                {return FLEncoder_Reset(_enc);}
    inline FLError Encoder::error() const       {return FLEncoder_GetError(_enc);}
    inline const char* Encoder::errorMessage() const {return FLEncoder_GetErrorMessage(_enc);}

    inline fleece::alloc_slice Delta::create(Value old, FLSharedKeys oldSK,
                                             Value nuu, FLSharedKeys nuuSK) {
        return FLCreateDelta(old, oldSK, nuu, nuuSK);
    }
    inline bool Delta::create(Value old, FLSharedKeys oldSK,
                              Value nuu, FLSharedKeys nuuSK,
                              Encoder &jsonEncoder) {
        return FLEncodeDelta(old, oldSK, nuu, nuuSK, jsonEncoder);
    }
    inline fleece::alloc_slice Delta::apply(Value old,
                                            FLSharedKeys sk,
                                            fleece::slice jsonDelta,
                                            FLError *error) {
        return FLApplyDelta(old, sk, jsonDelta, error);
    }
    inline bool Delta::apply(Value old,
                             FLSharedKeys sk,
                             Value jsonDelta,
                             Encoder &encoder) {
        return FLEncodeApplyingDelta(old, sk, jsonDelta, encoder);
    }
}

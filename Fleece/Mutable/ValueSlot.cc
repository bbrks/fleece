//
// ValueSlot.cc
//
// Copyright © 2018 Couchbase. All rights reserved.
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

#include "ValueSlot.hh"
#include "HeapArray.hh"
#include "HeapDict.hh"
#include "varint.hh"
#include <algorithm>

namespace fleece { namespace internal {
    using namespace std;
    using namespace internal;


    ValueSlot::ValueSlot(Null)
    :_inlineData{(kSpecialTag << 4) | kSpecialValueNull}
    ,_isInline(true)
    {
        static_assert(sizeof(ValueSlot) == 2*sizeof(void*), "ValueSlot is wrong size");
        static_assert(offsetof(ValueSlot, _inlineData) + ValueSlot::kInlineCapacity
                        == offsetof(ValueSlot, _isInline), "kInlineCapacity is wrong");
    }


    ValueSlot::ValueSlot(HeapCollection *md)
    :_asValue( retain(md)->asValue() )
    { }



    ValueSlot::ValueSlot(const ValueSlot &other) noexcept {
        *this = other; // invoke operator=, below
    }


    ValueSlot& ValueSlot::operator= (const ValueSlot &other) noexcept {
        releaseValue();
        _isInline = other._isInline;
        if (_isInline)
            memcpy(&_inlineData, &other._inlineData, kInlineCapacity);
        else
            _asValue = retain(other._asValue);
        return *this;
    }


    ValueSlot::ValueSlot(ValueSlot &&other) noexcept {
        _isInline = other._isInline;
        if (_isInline) {
            memcpy(&_inlineData, &other._inlineData, kInlineCapacity);
        } else {
            _asValue = other._asValue;
            other._asValue = nullptr;
        }
    }

    ValueSlot& ValueSlot::operator= (ValueSlot &&other) noexcept {
        releaseValue();
        _isInline = other._isInline;
        if (_isInline) {
            memcpy(&_inlineData, &other._inlineData, kInlineCapacity);
        } else {
            _asValue = other._asValue;
            other._asValue = nullptr;
        }
        return *this;
    }



    ValueSlot::~ValueSlot() {
        if (!_isInline)
            release(_asValue);
    }


    Retained<HeapCollection> HeapCollection::mutableCopy(const Value *v, tags ifType) {
        if (!v || v->tag() != ifType)
            return nullptr;
        if (v->isMutable())
            return (HeapCollection*)asHeapValue(v);
        switch (ifType) {
            case kArrayTag: return new HeapArray((const Array*)v);
            case kDictTag:  return new HeapDict((const Dict*)v);
            default:        return nullptr;
        }
    }


    void ValueSlot::releaseValue() {
        if (!_isInline) {
            release(_asValue);
            _asValue = nullptr;
        }
    }


    const Value* ValueSlot::asValue() const {
        return _isInline ? (const Value*)&_inlineData : _asValue;
    }

    const Value* ValueSlot::asValueOrUndefined() const {
        if (_isInline)
            return (const Value*)&_inlineData;
        else if (_asValue)
            return _asValue;
        else
            return Value::kUndefinedValue;
    }


    void ValueSlot::setInline(internal::tags valueTag, int tiny) {
        releaseValue();
        _isInline = true;
        _inlineData[0] = uint8_t((valueTag << 4) | tiny);
    }

    void ValueSlot::set(Null) {
        setInline(kSpecialTag, kSpecialValueNull);
    }


    void ValueSlot::set(bool b) {
        setInline(kSpecialTag, b ? kSpecialValueTrue : kSpecialValueFalse);
    }


    void ValueSlot::set(int i)       {setInt(i, false);}
    void ValueSlot::set(unsigned i)  {setInt(i, true);}
    void ValueSlot::set(int64_t i)   {setInt(i, false);}
    void ValueSlot::set(uint64_t i)  {setInt(i, true);}

    template <class INT>
    void ValueSlot::setInt(INT i, bool isUnsigned) {
        if (i < 2048 && (isUnsigned || -i < 2048)) {
            setInline(kShortIntTag, (i >> 8) & 0x0F);
            _inlineData[1] = (uint8_t)(i & 0xFF);
        } else {
            uint8_t buf[8];
            auto size = PutIntOfLength(buf, i, isUnsigned);
            setValue(kIntTag,
                     (int)(size-1) | (isUnsigned ? 0x08 : 0),
                     {buf, size});
        }
    }


    void ValueSlot::set(float f) {
        littleEndianFloat lf(f);
        setValue(kFloatTag, 0, {&lf, sizeof(lf)});
    }

    void ValueSlot::set(double d) {
        littleEndianDouble ld(d);
        setValue(kFloatTag, 8, {&ld, sizeof(ld)});
    }


    void ValueSlot::setValue(const Value *v) {
        if (!_isInline) {
            if (v == _asValue)
                return;
            release(_asValue);
        }
        if (v && v->tag() < kArrayTag) {
            auto size = v->dataSize();
            if (size <= kInlineCapacity) {
                // Copy value inline if it's small enough
                _isInline = true;
                memcpy(&_inlineData, v, size);
                return;
            }
        }
        // else point to it
        _isInline = false;
        _asValue = retain(v);
    }


    void ValueSlot::setValue(tags valueTag, int tiny, slice bytes) {
        releaseValue();
        if (1 + bytes.size <= kInlineCapacity) {
            _inlineData[0] = uint8_t((valueTag << 4) | tiny);
            memcpy(&_inlineData[1], bytes.buf, bytes.size);
            _isInline = true;
        } else {
            _asValue = retain(HeapValue::create(valueTag, tiny, bytes)->asValue());
            _isInline = false;
        }
    }


    void ValueSlot::_setStringOrData(tags valueTag, slice s) {
        if (s.size + 1 <= kInlineCapacity) {
            // Short strings can go inline:
            setInline(valueTag, (int)s.size);
            memcpy(&_inlineData[1], s.buf, s.size);
        } else {
            releaseValue();
            _asValue = retain(HeapValue::createStr(valueTag, s)->asValue());
            _isInline = false;
        }
    }


    HeapCollection* ValueSlot::makeMutable(tags ifType) {
        if (_isInline)
            return nullptr;
        Retained<HeapCollection> mval = HeapCollection::mutableCopy(_asValue, ifType);
        if (mval)
            set(mval->asValue());
        return mval;
    }

} }

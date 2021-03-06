//
// HeapValue.cc
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

#include "HeapValue.hh"
#include "varint.hh"
#include <algorithm>

namespace fleece { namespace internal {
    using namespace std;


    void* HeapValue::operator new(size_t size, size_t valueSize) {
        static_assert(offsetof(HeapValue, _header) & 1, "_header must be at odd address");
        return ::operator new(size + valueSize);
    }


    HeapValue::HeapValue(tags tag, int tiny) {
        _pad = 0xFF;
        _header = uint8_t((tag << 4) | tiny);
    }


    HeapValue* HeapValue::create(tags tag, int tiny, slice extraData) {
        auto hv = new (extraData.size) HeapValue(tag, tiny);
        memcpy(&hv->_header + 1, extraData.buf, extraData.size);
        return hv;
    }


    HeapValue* HeapValue::create(Null) {
        return new (0) HeapValue(kSpecialTag, kSpecialValueNull);
    }

    HeapValue* HeapValue::create(bool b) {
        return new (0) HeapValue(kSpecialTag, b ? kSpecialValueTrue : kSpecialValueFalse);
    }

    template <class INT>
    HeapValue* HeapValue::createInt(INT i, bool isUnsigned) {
        if (i < 2048 && (isUnsigned || -i < 2048)) {
            uint8_t extra = (uint8_t)(i & 0xFF);
            return create(kShortIntTag,
                          (i >> 8) & 0x0F,
                          {&extra, 1});
        } else {
            uint8_t buf[8];
            auto size = PutIntOfLength(buf, i, isUnsigned);
            return create(kIntTag,
                          (int)(size-1) | (isUnsigned ? 0x08 : 0),
                          {buf, size});
        }
    }


    HeapValue* HeapValue::create(float f) {
        littleEndianFloat lf(f);
        return create(kFloatTag, 0, {&lf, sizeof(lf)});
    }

    HeapValue* HeapValue::create(double d) {
        littleEndianDouble ld(d);
        return create(kFloatTag, 8, {&ld, sizeof(ld)});
    }


    HeapValue* HeapValue::createStr(tags valueTag, slice s) {
        uint8_t sizeBuf[kMaxVarintLen32];
        size_t sizeByteCount = 0;
        int tiny;
        if (s.size < 0x0F) {
            tiny = (int)s.size;
        } else {
            tiny = 0x0F;
            sizeByteCount = PutUVarInt(&sizeBuf, s.size);
        }
        auto hv = new (sizeByteCount + s.size) HeapValue(valueTag, tiny);
        memcpy(&hv->_data[0], sizeBuf, sizeByteCount);
        memcpy(&hv->_data[sizeByteCount], s.buf, s.size);
        return hv;
    }


    HeapValue* HeapValue::create(const Value *v) {
        assert(v->tag() < kArrayTag);
        size_t size = v->dataSize();
        auto hv = new (size - 1) HeapValue();
        memcpy(&hv->_header, v, size);
        return hv;
    }


    HeapValue* HeapValue::asHeapValue(const Value *v) {
        if (!isHeapValue(v))
            return nullptr;
        auto ov = (offsetValue*)(size_t(v) & ~1);
        assert(ov->_pad == 0xFF);
        return (HeapValue*)ov;
    }


    void HeapValue::retain(const Value *v) {
        fleece::retain(HeapValue::asHeapValue(v));
    }

    void HeapValue::release(const Value *v) {
        fleece::release(HeapValue::asHeapValue(v));
    }

} }

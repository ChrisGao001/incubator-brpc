// Copyright (c) 2018 Baidu, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Why LZ4 compress use two buffer blocks? You can refer to more information.
// https://github.com/lz4/lz4/blob/dev/examples/blockStreaming_doubleBuffer.md

// Authors: HaoPeng,Li (happenlee@hotmail.com)

#include "butil/logging.h"
#include "butil/third_party/lz4/lz4.h"
#include "brpc/policy/lz4_compress.h"
#include "brpc/protocol.h"

namespace brpc {
namespace policy {

char* EncodeVarint32(char* dst, uint32_t v) {
  // Operate on characters as unsigneds
  unsigned char* ptr = reinterpret_cast<unsigned char*>(dst);
  static const int B = 128;
  if (v < (1 << 7)) {
    *(ptr++) = v;
  } else if (v < (1 << 14)) {
    *(ptr++) = v | B;
    *(ptr++) = v >> 7;
  } else if (v < (1 << 21)) {
    *(ptr++) = v | B;
    *(ptr++) = (v >> 7) | B;
    *(ptr++) = v >> 14;
  } else if (v < (1 << 28)) {
    *(ptr++) = v | B;
    *(ptr++) = (v >> 7) | B;
    *(ptr++) = (v >> 14) | B;
    *(ptr++) = v >> 21;
  } else {
    *(ptr++) = v | B;
    *(ptr++) = (v >> 7) | B;
    *(ptr++) = (v >> 14) | B;
    *(ptr++) = (v >> 21) | B;
    *(ptr++) = v >> 28;
  }
  return reinterpret_cast<char*>(ptr);
}

const char* GetVarint32PtrFallback(const char* p, const char* limit,
                                   uint32_t* value) {
  uint32_t result = 0;
  for (uint32_t shift = 0; shift <= 28 && p < limit; shift += 7) {
    uint32_t byte = *(reinterpret_cast<const unsigned char*>(p));
    p++;
    if (byte & 128) {
      // More bytes are present
      result |= ((byte & 127) << shift);
    } else {
      result |= (byte << shift);
      *value = result;
      return reinterpret_cast<const char*>(p);
    }
  }
  return nullptr;
}

inline const char* GetVarint32Ptr(const char* p,
                                  const char* limit,
                                  uint32_t* value) {
  if (p < limit) {
    uint32_t result = *(reinterpret_cast<const unsigned char*>(p));
    if ((result & 128) == 0) {
      *value = result;
      return p + 1;
    }
  }
  return GetVarint32PtrFallback(p, limit, value);
}

bool LZ4Compress(const butil::IOBuf& data, butil::IOBuf* out) {
  butil::IOBufBytesIterator in(data);
  char inp_buf[2][BLOCK_BYTES];
  int  inp_buf_index = 0;
  for (;;) {
    char* inp_ptr = inp_buf[inp_buf_index];
    const int inp_bytes = in.copy_and_forward(inp_ptr, BLOCK_BYTES);
    if (0 == inp_bytes) {
      break;
    }
    char cmp_buf[butil::LZ4_compressBound(BLOCK_BYTES)];
    size_t compressed_size = butil::LZ4_compress(inp_ptr, cmp_buf, inp_bytes);
    char len_code[5] = {0};
    char* ptr = EncodeVarint32(len_code, compressed_size);
    out->append(&len_code, ptr - len_code);
    out->append(&cmp_buf, compressed_size);
    inp_buf_index = (inp_buf_index + 1) % 2;
  }
  const int end = 0;
  out->append(&end, sizeof(end));
  return true;
}

bool LZ4Decompress(const butil::IOBuf& data, butil::IOBuf* out) {
  butil::IOBufBytesIterator in(data);
  char dec_buf[2][BLOCK_BYTES];
  int  dec_buf_index = 0;
  for (;;) {
    char cmp_buf[LZ4_COMPRESSBOUND(BLOCK_BYTES)];
    uint32_t cmp_bytes = 0;
    int i = 0;
    for(;;) {
      size_t nc = in.copy_and_forward(&cmp_buf[i], 1);
      if (nc == 0) {
        return true;
      }
      if (!(cmp_buf[i] & 0x80)) {
        ++i;
        break;
      }
      ++i;
    }
    if (cmp_buf[0] == 0x00) {
      break;
    }
    GetVarint32Ptr(cmp_buf, cmp_buf + 5, &cmp_bytes);
    size_t nc = in.copy_and_forward(&cmp_buf, cmp_bytes);
    if (nc < cmp_bytes) {
      LOG(WARNING) << "LZ4 Uncompress failed";
      return false;
    }
    char* const dec_ptr = dec_buf[dec_buf_index];
    int dec_bytes = butil::LZ4_uncompress_unknownOutputSize(cmp_buf, dec_ptr,
                                                            cmp_bytes,
                                                            BLOCK_BYTES);
    if (dec_bytes <= 0) {
      LOG(WARNING) << "LZ4 Uncompress failed";
      return false;
    }
    out->append(dec_ptr, dec_bytes);
    dec_buf_index = (dec_buf_index + 1) % 2;
  }
  return true;
}


bool LZ4Compress(const google::protobuf::Message& res, butil::IOBuf* buf) {
  butil::IOBuf in;
  butil::IOBufAsZeroCopyOutputStream wrapper(&in);
  if (res.SerializeToZeroCopyStream(&wrapper)) {
    return LZ4Compress(in, buf);
  }
  LOG(WARNING) << "Fail to serialize input pb=" << &res;
  return false;
}

bool LZ4Decompress(const butil::IOBuf& data, google::protobuf::Message* req) {
  butil::IOBuf out;
  if (LZ4Decompress(data, &out) && ParsePbFromIOBuf(req, out)) {
    return true;
  }
  LOG(WARNING) << "Fail to Uncompress, size=" << data.size();
  return false;
}

}  // namespace policy
} // namespace

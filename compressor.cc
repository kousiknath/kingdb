// Copyright (c) 2014, Emmanuel Goossaert. All rights reserved.
// Use of this source code is governed by the BSD 3-Clause License,
// that can be found in the LICENSE file.

#include "compressor.h"

namespace kdb {

void CompressorLZ4::Reset() {
  ts_compress_.reset();
  ts_uncompress_.reset();
}


Status CompressorLZ4::Compress(char *source,
                               uint64_t size_source,
                               char **dest,
                               uint64_t *size_dest) {
  if (size_source < 8) {
    *dest = nullptr;
    *size_dest = 0;
    return Status::OK();
  }
  uint32_t bound = LZ4_compressBound(size_source);
  *size_dest = 0;
  *dest = new char[8 + bound];

  int ret = LZ4_compress_limitedOutput(source, (*dest) + 8, size_source, bound);
  if (ret <= 0) {
    delete[] *dest;
    return Status::IOError("LZ4_compress_limitedOutput() failed");
  }
  uint32_t size_compressed = ret + 8;
  uint32_t size_source_32 = size_source; // TODO: careful here, storing 64 bits into 32.
  memcpy((*dest),     &size_compressed, sizeof(size_compressed));
  memcpy((*dest) + 4, &size_source_32,  sizeof(size_source_32));
  LOG_TRACE("CompressorLZ4::Compress()", "size_compressed:%u size_source:%u", size_compressed, size_source_32);
  uint64_t size_compressed_total = ts_compress_.get() + size_compressed;
  ts_compress_.put(size_compressed_total);
  *size_dest = size_compressed;
  return Status::OK();
}

Status CompressorLZ4::Uncompress(char *source,
                                 uint64_t size_source_total,
                                 char **dest,
                                 uint64_t *size_dest) {
  uint64_t offset_uncompress = ts_uncompress_.get();
  LOG_TRACE("CompressorLZ4::Uncompress()", "in %llu %llu", offset_uncompress, size_source_total);
  if (offset_uncompress == size_source_total) return Status::Done();

  uint32_t size_source, size_compressed;
  memcpy(&size_compressed, source + offset_uncompress,     sizeof(size_compressed));
  memcpy(&size_source,     source + 4 + offset_uncompress, sizeof(size_source));
  size_compressed -= 8;

  *size_dest = 0;
  *dest = new char[size_source];
  int size = size_compressed;
  LOG_TRACE("CompressorLZ4::Uncompress()", "ptr:%p size:%d size_source:%d offset:%llu", source + offset_uncompress + 8, size, size_source, offset_uncompress);
  int ret = LZ4_decompress_safe_partial(source + offset_uncompress + 8,
                                        *dest,
                                        size,
                                        size_source,
                                        size_source);
  if (ret <= 0) {
    delete[] (*dest);
    return Status::IOError("LZ4_decompress_safe_partial() failed");
  }

  offset_uncompress += size_compressed + 8;
  ts_uncompress_.put(offset_uncompress);

  LOG_TRACE("CompressorLZ4::Uncompress()", "out %llu %llu", offset_uncompress, size_source_total);
  *size_dest = ret;
  return Status::OK();
}

};
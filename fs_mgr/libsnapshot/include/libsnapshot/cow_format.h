// Copyright (C) 2019 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include <stdint.h>
#include <string>

namespace android {
namespace snapshot {

static constexpr uint64_t kCowMagicNumber = 0x436f77634f572121ULL;
static constexpr uint32_t kCowVersionMajor = 1;
static constexpr uint32_t kCowVersionMinor = 0;

// This header appears as the first sequence of bytes in the COW. All fields
// in the layout are little-endian encoded. The on-disk layout is:
//
//      +-----------------------+
//      |     Header (fixed)    |
//      +-----------------------+
//      | Operation  (variable) |
//      | Data       (variable) |
//      +-----------------------+
//      |    Footer (fixed)     |
//      +-----------------------+
//
// The operations begin immediately after the header, and the "raw data"
// immediately follows the operation which refers to it. While streaming
// an OTA, we can immediately write the op and data, syncing after each pair,
// while storing operation metadata in memory. At the end, we compute data and
// hashes for the footer, which is placed at the tail end of the file.
//
// A missing or corrupt footer likely indicates that writing was cut off
// between writing the last operation/data pair, or the footer itself. In this
// case, the safest way to proceed is to assume the last operation is faulty.

struct CowHeader {
    uint64_t magic;
    uint16_t major_version;
    uint16_t minor_version;

    // Size of this struct.
    uint16_t header_size;

    // Size of footer struct
    uint16_t footer_size;

    // The size of block operations, in bytes.
    uint32_t block_size;
} __attribute__((packed));

// This structure is the same size of a normal Operation, but is repurposed for the footer.
struct CowFooterOperation {
    // The operation code (always kCowFooterOp).
    uint8_t type;

    // If this operation reads from the data section of the COW, this contains
    // the compression type of that data (see constants below).
    uint8_t compression;

    // Length of Footer Data. Currently 64 for both checksums
    uint16_t data_length;

    // The amount of file space used by Cow operations
    uint64_t ops_size;

    // The number of cow operations in the file
    uint64_t num_ops;
} __attribute__((packed));

struct CowFooterData {
    // SHA256 checksums of Footer op
    uint8_t footer_checksum[32];

    // SHA256 of the operation sequence.
    uint8_t ops_checksum[32];
} __attribute__((packed));

// Cow operations are currently fixed-size entries, but this may change if
// needed.
struct CowOperation {
    // The operation code (see the constants and structures below).
    uint8_t type;

    // If this operation reads from the data section of the COW, this contains
    // the compression type of that data (see constants below).
    uint8_t compression;

    // If this operation reads from the data section of the COW, this contains
    // the length.
    uint16_t data_length;

    // The block of data in the new image that this operation modifies.
    uint64_t new_block;

    // The value of |source| depends on the operation code.
    //
    // For copy operations, this is a block location in the source image.
    //
    // For replace operations, this is a byte offset within the COW's data
    // section (eg, not landing within the header or metadata). It is an
    // absolute position within the image.
    //
    // For zero operations (replace with all zeroes), this is unused and must
    // be zero.
    //
    // For Label operations, this is the value of the applied label.
    uint64_t source;
} __attribute__((packed));

static_assert(sizeof(CowOperation) == sizeof(CowFooterOperation));

static constexpr uint8_t kCowCopyOp = 1;
static constexpr uint8_t kCowReplaceOp = 2;
static constexpr uint8_t kCowZeroOp = 3;
static constexpr uint8_t kCowLabelOp = 4;
static constexpr uint8_t kCowFooterOp = -1;

static constexpr uint8_t kCowCompressNone = 0;
static constexpr uint8_t kCowCompressGz = 1;
static constexpr uint8_t kCowCompressBrotli = 2;

struct CowFooter {
    CowFooterOperation op;
    CowFooterData data;
} __attribute__((packed));

std::ostream& operator<<(std::ostream& os, CowOperation const& arg);

int64_t GetNextOpOffset(const CowOperation& op);

}  // namespace snapshot
}  // namespace android
// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "util/atomic_write.h"

#include <string>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "util/test/test.h"

class ImportantFileWriterTest : public testing::Test {
 public:
  ImportantFileWriterTest() = default;
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    file_ = temp_dir_.GetPath().AppendASCII("test-file");
  }

 protected:
  base::FilePath file_;

 private:
  base::ScopedTempDir temp_dir_;
};

// Test that WriteFileAtomically works and that the chunking to avoid very large
// writes works.
TEST_F(ImportantFileWriterTest, WriteLargeFile) {
  // One byte larger than kMaxWriteAmount.
  const std::string large_data(8 * 1024 * 1024 + 1, 'g');
  EXPECT_FALSE(base::PathExists(file_));
  EXPECT_TRUE(util::WriteFileAtomically(file_, large_data.data(), large_data.size()));
  std::string actual;
  EXPECT_TRUE(ReadFileToString(file_, &actual));
  EXPECT_EQ(large_data, actual);
}

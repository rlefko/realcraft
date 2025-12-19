// RealCraft Platform Tests
// file_io_test.cpp - File I/O unit tests

#include <gtest/gtest.h>
#include <realcraft/platform/file_io.hpp>

#include <cstdlib>
#include <fstream>

using namespace realcraft::platform;

class FileIOTest : public ::testing::Test {
protected:
    fs::path test_dir_;
    fs::path test_file_;

    void SetUp() override {
        test_dir_ = FileSystem::get_temp_directory() / "realcraft_test";
        FileSystem::create_directories(test_dir_);
        test_file_ = test_dir_ / "test_file.txt";
    }

    void TearDown() override {
        FileSystem::remove_all(test_dir_);
    }
};

TEST_F(FileIOTest, GetExecutableDirectory) {
    auto dir = FileSystem::get_executable_directory();
    EXPECT_TRUE(dir.is_absolute());
    EXPECT_TRUE(FileSystem::exists(dir));
}

TEST_F(FileIOTest, GetTempDirectory) {
    auto dir = FileSystem::get_temp_directory();
    // Just verify it returns a valid path
    EXPECT_FALSE(dir.empty());
}

TEST_F(FileIOTest, CreateDirectories) {
    auto nested = test_dir_ / "a" / "b" / "c";
    EXPECT_TRUE(FileSystem::create_directories(nested));
    EXPECT_TRUE(FileSystem::exists(nested));
    EXPECT_TRUE(FileSystem::is_directory(nested));
}

TEST_F(FileIOTest, WriteAndReadText) {
    std::string content = "Hello, RealCraft!\nLine 2\n";

    EXPECT_TRUE(FileSystem::write_text(test_file_, content));
    EXPECT_TRUE(FileSystem::exists(test_file_));
    EXPECT_TRUE(FileSystem::is_file(test_file_));

    auto result = FileSystem::read_text(test_file_);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, content);
}

TEST_F(FileIOTest, WriteAndReadBinary) {
    std::vector<uint8_t> data = {0x00, 0x01, 0x02, 0x03, 0xFF, 0xFE, 0xFD};

    EXPECT_TRUE(FileSystem::write_binary(test_file_, data));
    EXPECT_TRUE(FileSystem::exists(test_file_));

    auto result = FileSystem::read_binary(test_file_);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, data);
}

TEST_F(FileIOTest, ReadNonexistentFile) {
    auto result = FileSystem::read_text(test_dir_ / "nonexistent.txt");
    EXPECT_FALSE(result.has_value());
}

TEST_F(FileIOTest, FileSize) {
    std::string content = "Test content 123";
    FileSystem::write_text(test_file_, content);

    auto size = FileSystem::file_size(test_file_);
    ASSERT_TRUE(size.has_value());
    EXPECT_EQ(*size, content.size());
}

TEST_F(FileIOTest, RemoveFile) {
    FileSystem::write_text(test_file_, "test");
    EXPECT_TRUE(FileSystem::exists(test_file_));

    EXPECT_TRUE(FileSystem::remove(test_file_));
    EXPECT_FALSE(FileSystem::exists(test_file_));
}

TEST_F(FileIOTest, RemoveAll) {
    auto nested = test_dir_ / "to_remove" / "nested";
    FileSystem::create_directories(nested);
    FileSystem::write_text(nested / "file.txt", "content");

    EXPECT_TRUE(FileSystem::remove_all(test_dir_ / "to_remove"));
    EXPECT_FALSE(FileSystem::exists(test_dir_ / "to_remove"));
}

TEST_F(FileIOTest, ListDirectory) {
    FileSystem::write_text(test_dir_ / "file1.txt", "1");
    FileSystem::write_text(test_dir_ / "file2.txt", "2");
    FileSystem::create_directories(test_dir_ / "subdir");

    auto entries = FileSystem::list_directory(test_dir_);
    EXPECT_GE(entries.size(), 3u);  // At least file1, file2, subdir
}

TEST_F(FileIOTest, ListFilesWithExtension) {
    FileSystem::write_text(test_dir_ / "file1.txt", "1");
    FileSystem::write_text(test_dir_ / "file2.txt", "2");
    FileSystem::write_text(test_dir_ / "file3.dat", "3");

    auto txt_files = FileSystem::list_files(test_dir_, ".txt");
    EXPECT_EQ(txt_files.size(), 2u);

    auto dat_files = FileSystem::list_files(test_dir_, ".dat");
    EXPECT_EQ(dat_files.size(), 1u);
}

TEST_F(FileIOTest, PathUtilities) {
    fs::path relative("relative/path");
    EXPECT_FALSE(FileSystem::is_absolute(relative));

    fs::path absolute = FileSystem::make_absolute(relative);
    EXPECT_TRUE(FileSystem::is_absolute(absolute));
}

TEST_F(FileIOTest, LastModified) {
    FileSystem::write_text(test_file_, "initial");

    auto time1 = FileSystem::last_modified(test_file_);
    ASSERT_TRUE(time1.has_value());

    // Small delay to ensure different timestamp
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    FileSystem::write_text(test_file_, "updated");

    auto time2 = FileSystem::last_modified(test_file_);
    ASSERT_TRUE(time2.has_value());

    EXPECT_GE(*time2, *time1);
}

class ResourceCacheTest : public ::testing::Test {
protected:
    ResourceCache cache_;

    void SetUp() override {
        cache_.set_max_size(1024 * 1024);  // 1 MB
    }
};

TEST_F(ResourceCacheTest, PutAndGet) {
    std::vector<uint8_t> data = {1, 2, 3, 4, 5};
    cache_.put("key1", data);

    auto result = cache_.get("key1");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->size(), data.size());
    EXPECT_EQ((*result)[0], data[0]);
}

TEST_F(ResourceCacheTest, GetNonexistent) {
    auto result = cache_.get("nonexistent");
    EXPECT_FALSE(result.has_value());
}

TEST_F(ResourceCacheTest, Invalidate) {
    std::vector<uint8_t> data = {1, 2, 3};
    cache_.put("key1", data);
    EXPECT_TRUE(cache_.get("key1").has_value());

    cache_.invalidate("key1");
    EXPECT_FALSE(cache_.get("key1").has_value());
}

TEST_F(ResourceCacheTest, Clear) {
    cache_.put("key1", std::vector<uint8_t>{1, 2, 3});
    cache_.put("key2", std::vector<uint8_t>{4, 5, 6});

    EXPECT_EQ(cache_.entry_count(), 2u);

    cache_.clear();

    EXPECT_EQ(cache_.entry_count(), 0u);
    EXPECT_EQ(cache_.current_size(), 0u);
}

TEST_F(ResourceCacheTest, CurrentSize) {
    EXPECT_EQ(cache_.current_size(), 0u);

    std::vector<uint8_t> data(100, 0);
    cache_.put("key1", data);

    EXPECT_EQ(cache_.current_size(), 100u);

    cache_.put("key2", data);
    EXPECT_EQ(cache_.current_size(), 200u);
}

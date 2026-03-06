#ifndef FILE_IO_MOCKS_H
#define FILE_IO_MOCKS_H

#include <gmock/gmock.h>
#include <string>
#include <fstream>
#include <memory>

// Mock file system operations
class MockFileSystem {
public:
    MOCK_METHOD(bool, FileExists, (const std::string& path));
    MOCK_METHOD(bool, DirectoryExists, (const std::string& path));
    MOCK_METHOD(bool, CreateDirectory, (const std::string& path));
    MOCK_METHOD(std::string, ReadFile, (const std::string& path));
    MOCK_METHOD(bool, WriteFile, (const std::string& path, const std::string& content));
};

// Mock stream for testing file operations
class MockStream {
public:
    virtual ~MockStream() = default;
    MOCK_METHOD(bool, is_open, ());
    MOCK_METHOD(bool, good, ());
    MOCK_METHOD(bool, eof, ());
    MOCK_METHOD(std::string, getline, ());
};

#endif // FILE_IO_MOCKS_H

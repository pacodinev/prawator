#pragma once

#include <cassert>
#include <fcntl.h>
#ifdef __unix__

#include <unistd.h>

#include <memory>
#include <cstdint>
#include <cerrno>
#include <stdexcept>
#include <exception>
#include <system_error>

// a faster alternative to std::fstream
class PosixFostream {
private:
    std::unique_ptr<std::uint8_t[]> buffer; // NOLINT
    std::size_t size, used{0};
    int m_ffd;

    void cppwrite(const std::uint8_t* buf, std::size_t bufSize) {
        ssize_t ret = 0;
        while(bufSize > 0) {
            ret = ::write(m_ffd, buf, bufSize);
            if(ret < 0 && errno == EINTR) {
                continue;
            }
            if(ret < 0) {
                throw std::system_error(errno, std::system_category(), "write syscall for map save failed");
            }
            buf += ret; bufSize -= static_cast<std::size_t>(ret);  // NOLINT
        }
    }

public:
    // if bufferSize == 0, then no buffering takes place
    // takes ownership of ffd
    PosixFostream(int ffd, std::size_t bufferSize) 
        : size(bufferSize), m_ffd(ffd) {
        assert(m_ffd > 0);
        if(bufferSize > 0) {
            buffer = std::make_unique<std::uint8_t[]>(bufferSize); // NOLINT
        }
    }

    // if bufferSize == 0, then no buffering takes place
    // takes ownership of ffd
    PosixFostream(const char path[], int flags, int mode, std::size_t bufferSize) // NOLINT
        : size(bufferSize) {
        m_ffd = ::open(path, flags, mode); // NOLINT
        if(m_ffd < 0) {
            throw std::system_error(errno, std::system_category(), 
                    "Faild calling open on file" + std::string{path});
        }
        if(bufferSize > 0) {
            buffer = std::make_unique<std::uint8_t[]>(bufferSize); // NOLINT
        }
    }
    
    PosixFostream(const PosixFostream&) = delete;
    PosixFostream& operator=(const PosixFostream&) = delete;
    PosixFostream(PosixFostream&& other) noexcept
        : buffer(std::move(other.buffer)),
          size(other.size), used(other.used), 
          m_ffd(other.m_ffd) {
        other.size = 0;
        other.used = 0;
        other.m_ffd = -1;
    }
    PosixFostream& operator= (PosixFostream&& other) noexcept {
        buffer = std::move(other.buffer);
        size = other.size;
        used = other.used;
        m_ffd = other.m_ffd;

        other.size = 0;
        other.used = 0;
        other.m_ffd = -1;

        return *this;
    }

    ~PosixFostream() noexcept {
        if(used > 0) {
            try {
                cppwrite(buffer.get(), used);
            } catch(...) { }
            used = 0;
        }
        ::close(m_ffd);
    }

    void write(const std::uint8_t* buf, std::size_t bufSize) {
        if(buffer == nullptr) {
            cppwrite(buf, bufSize);
            return;
        }
        while(bufSize > 0) {
            std::size_t curSize = std::min(bufSize, size - used);
            std::copy(buf, buf+curSize, buffer.get()+used); // NOLINT
            bufSize -= curSize; buf += curSize; // NOLINT
            used += curSize;
            if(used == size) {
                cppwrite(buffer.get(), used);
                used = 0;
            }
        }
    }

    template<class T>
    void write(const T &val) {
        write(reinterpret_cast<const std::uint8_t*>(&val), sizeof(T)); // NOLINT
    }

    void flush() {
        if(used > 0) {
            cppwrite(buffer.get(), used);
            used = 0;
        }
    }
};
#endif

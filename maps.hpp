#pragma once
#include <string>
#include <base64_rfc4648.hpp>
#include <iostream>
#include <sstream>

/*
"Mostly ASCII printable stuff" encoding.

This binary encoding scheme is aimed at preserving
printable ASCII characters while encoding the rest
as blocks of base64 strings
*/

namespace maps {

using base64 = cppcodec::base64_rfc4648;

class DecodingError : public std::runtime_error {
public:
    DecodingError(std::string what) :
        std::runtime_error(what)
    {}
};

constexpr bool is_binary(const char& c) {
    return !((' ' <= c && c <= '~') ||
        c == '\n' || c == '\r' || c == '\t');
}

constexpr bool is_base64_char(const char& c) {
    return ('A' <= c && c <= 'Z') ||
           ('a' <= c && c <= 'z') ||
           ('0' <= c && c <= '9') ||
           c == '+' || c == '/' || c == '=';
}

constexpr char block_open = '{';
constexpr char block_close = '}';

template<typename It>
std::ostringstream encode(It it, It end) {
    std::ostringstream output;
    std::string blob_buffer;
    for (; it != end; ++it) {
        const char& c = *it;
        const bool char_is_bin = is_binary(c);
        const bool buffer_waiting = !blob_buffer.empty();
        if (char_is_bin) {
            blob_buffer.push_back(c);
            continue;
        } else if (buffer_waiting) {
            output << block_open <<
                      base64::encode(blob_buffer) <<
                      block_close;
            blob_buffer.clear();
        }
        if (c == block_open || c == block_close)
            output << c;
        output << c;
    }
    if (!blob_buffer.empty()) {
        output << block_open <<
                  base64::encode(blob_buffer) <<
                  block_close;
    }
    return output;
}

std::string encode(const std::string& input) {
    return encode(input.begin(), input.end()).str();
}

// Reads a binary blob's contents starting at ``it``
// advances ``it`` until the closing bracket
template<typename It>
std::string parse_blob(It& it, const It& end) {
    std::ostringstream blob;
    if (it != end && *it == block_open)
        // This isn't a blob, it's an escape sequence
        return std::string(1, block_open);
    if (it != end && *it == block_close)
        throw DecodingError(
            "Empty base64 blocks are not allowed");
    while (it != end && is_base64_char(*it))
        blob << *(it++);
    if (it == end)
        throw DecodingError(
            "base64 block ended prematurely, "
            "expected closing symbol.");
    if (*it != block_close)
        throw DecodingError(
            "Only base64 chars are allowed inside "
            "base64 blocks.");
    std::vector<uint8_t> decoded = base64::decode(
        blob.str());
    return std::string(decoded.begin(), decoded.end());
}

template<typename It>
std::ostringstream decode(It it, It end) {
    std::ostringstream output;
    for (;it != end; ++it) {
        const char& c = *it;
        if (c == block_open) {
            auto parsed = parse_blob(++it, end);
            output << parsed;
            continue;
        }
        if (c == block_close &&
                (++it == end || *it != block_close))
            throw DecodingError(
                "Escape sequence ended prematurely, "
                "expected closing symbol."
            );
        if (is_binary(c))
            throw DecodingError(
                "Non printable literals are not allowed."
            );
        output << c;
    }
    output.flush();
    return output;
}

std::string decode(const std::string& input) {
    return decode(
        std::begin(input), std::end(input)).str();
}

}

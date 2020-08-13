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
void encode(It it, It end,
            std::string &output_buffer,
            std::string &blob_buffer,
            std::string &base64e_buffer) {
    output_buffer.clear();
    for (; it != end; ++it) {
        const char& c = *it;
        const bool char_is_bin = is_binary(c);
        const bool buffer_waiting = !blob_buffer.empty();
        if (char_is_bin) {
            blob_buffer.push_back(c);
            continue;
        } else if (buffer_waiting) {
            base64::encode(base64e_buffer, blob_buffer);
            output_buffer.push_back(block_open);
            output_buffer.append(base64e_buffer);
            output_buffer.push_back(block_close);
            blob_buffer.clear();
        }
        if (c == block_open || c == block_close)
            output_buffer.push_back(c);
        output_buffer.push_back(c);
    }
    if (!blob_buffer.empty()) {
        base64::encode(base64e_buffer, blob_buffer);
        output_buffer.push_back(block_open);
        output_buffer.append(base64e_buffer);
        output_buffer.push_back(block_close);
    }
}

template<typename It>
std::string encode(It it, It end) {
    std::string output_buffer;
    std::string blob_buffer;
    std::string base64e_buffer;
    encode(it, end, output_buffer,
        blob_buffer, base64e_buffer);
    return output_buffer;
}

std::string encode(const std::string& input) {
    return encode(input.begin(), input.end());
}

// Reads a binary blob's contents starting at ``it``
// advances ``it`` until the closing bracket
template<typename It>
void parse_blob(It& it, const It& end,
                std::string &read_buffer,
                std::string &out_buffer,
                std::string &base64d_buffer) {
    read_buffer.clear();
    if (it != end && *it == block_open) {
        // This isn't a blob, it's an escape sequence
        out_buffer.push_back(block_open);
        return;
    }
    if (it != end && *it == block_close)
        throw DecodingError(
            "Empty base64 blocks are not allowed");
    while (it != end && is_base64_char(*it))
        read_buffer.push_back(*(it++));
    if (it == end)
        throw DecodingError(
            "base64 block ended prematurely, "
            "expected closing symbol.");
    if (*it != block_close)
        throw DecodingError(
            "Only base64 chars are allowed inside "
            "base64 blocks.");
    // TODO: Check that this writes to the end of the buffer
    base64::decode(base64d_buffer, read_buffer);
    out_buffer.append(base64d_buffer);
}

template<typename It>
auto parse_blob(It& it, const It& end) {
    std::string buffer;
    return parse_blob(it, end, buffer);
}

template<typename It>
void decode(It it, It end,
            std::string &output_buffer,
            std::string &blob_buffer,
            std::string &base64d_buffer) {
    output_buffer.clear();
    for (;it != end; ++it) {
        const char& c = *it;
        if (c == block_open) {
            parse_blob(++it, end, blob_buffer,
                output_buffer, base64d_buffer);
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
        output_buffer.push_back(c);
    }
}

template<typename It>
std::string decode(It it, It end) {
    std::string output_buffer;
    std::string blob_buffer;
    std::string base64d_buffer;
    decode<It>(it, end, output_buffer,
        blob_buffer, base64d_buffer);
    return output_buffer;
}

std::string decode(const std::string& input) {
    return decode(std::begin(input), std::end(input));
}

}

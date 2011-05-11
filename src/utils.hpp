#ifndef UTILS_INCLUDE_GUARD
#define UTILS_INCLUDE_GUARD

#include <string>
#include <istream>
#include <boost/cstdint.hpp>
typedef boost::uint32_t uint32_t;

#include "trtok_clean_entities_EntityCleaner"

using namespace std;

namespace trtok {

inline string unicode_to_utf8(basic_string<uint32_t> const &str)
{
    return clean_entities::EntityCleaner_unicode_to_char(str);
}

inline bool is_whitespace(uint32_t c) {
    return ((c >= 0x0009) && (c <= 0x000D)) ||
            (c == 0x0020) ||
            (c == 0x0085) ||
            (c == 0x00A0) ||
            (c == 0x1680) ||
            (c == 0x180E) ||
           ((c >= 0x2000) && c <= (0x200A)) ||
           ((c >= 0x2028) && (c <= 0x2029)) ||
            (c == 0x202F) ||
            (c == 0x205F) ||
            (c == 0x3000);
}

inline bool is_newline(uint32_t c) {
    return  (c == 0x0A) ||
            (c == 0x0D);
}

inline uint32_t utf8char_to_unicode(char const *buffer, size_t &offset) {
    // C++ char type is signed, so we cast the array to uint8_t,
    // so the values are interpreted as naturals
    uint8_t *ubuffer = (uint8_t*)buffer;

    if (ubuffer[offset] >> 7 == 0 /*0xxxxxxx*/) {
        // An ASCII singleton.
        uint32_t codepoint = ubuffer[offset];
        offset++;
        return codepoint;
    } else if (ubuffer[offset] >> 5 == 6 /*110xxxxx*/) {
        // A two-byte sequence
        uint32_t msb = ubuffer[offset] & 31 /*last 5 bits*/;
        uint32_t lsb = ubuffer[offset+1] & 63 /*last 6 bits*/;
        uint32_t codepoint = (msb << 6) | lsb;
        offset += 2;
        return codepoint;
    } else if (ubuffer[offset] >> 4 == 14 /*1110xxxx*/) {
        // A three-byte sequence
        uint32_t msb = ubuffer[offset] & 15 /*last 4 bits*/;
        uint32_t smsb = ubuffer[offset+1] & 63;
        uint32_t lsb = ubuffer[offset+2] & 63;
        offset += 3;
        return (msb << 12) | (smsb << 6) | lsb;
    } else if (ubuffer[offset] >> 3 == 30 /*11110xxx*/) {
        // A four-byte sequence
        uint32_t msb = ubuffer[offset] & 7; /*last 3 bits*/
        uint32_t smsb = ubuffer[offset+1] & 63;
        uint32_t slsb = ubuffer[offset+2] & 63;
        uint32_t lsb = ubuffer[offset+3] & 63;
        offset += 4;
        return (msb << 18) | (smsb << 12) | (slsb << 6) | lsb;
    } else {
        throw std::domain_error
            ("buffer does not hold a valid UTF-8 character.");
    }
}

inline basic_string<uint32_t> utf8_to_unicode(string const &str) {
    string::size_type i = 0;
    basic_string<uint32_t> codepoints;

    while (i < str.length()) {
        codepoints.push_back(utf8char_to_unicode(str.data(), i));
    }

    return codepoints;
}

inline uint32_t get_unicode_from_utf8(std::istream *input_stream_p) {
    // We use the char-typed pointer to interface with the STL
    // IOstreams and the uint8_t pointer to interpret the values
    // as naturals in our code
    char buffer[4];
    uint8_t  *ubuffer = (uint8_t*)buffer;

    size_t offset = 0;
    input_stream_p->get(buffer[0]);
    if (input_stream_p->gcount() == 0)
        return 0;
    if (ubuffer[0] >= 192) // 11xxxxxx => at least two bytes
        input_stream_p->get(buffer[1]);
    if (ubuffer[0] >= 224) // 111xxxxx => at least three bytes
        input_stream_p->get(buffer[2]);
    if (ubuffer[0] >= 240) // 1111xxxx => four bytes
        input_stream_p->get(buffer[3]);
    return utf8char_to_unicode(buffer, offset);
}

}
#endif

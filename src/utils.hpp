#ifndef UTILS_INCLUDE_GUARD
#define UTILS_INCLUDE_GUARD

#include <string>
#include <istream>
#include <boost/cstdint.hpp>
typedef boost::uint32_t uint32_t;

#include "trtok_clean_entities_EntityCleaner"

namespace trtok {

inline std::string unicode_to_utf8(std::basic_string<uint32_t> const &str)
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
    if (buffer[offset] >> 7 == 0 /*0xxxxxxx*/) {
        // An ASCII singleton.
        uint32_t codepoint = buffer[offset];
        offset++;
        return codepoint;
    } else if (buffer[offset] >> 5 == 6 /*110xxxxx*/) {
        // A two-byte sequence
        uint32_t msb = buffer[offset] & 31 /*last 5 bits*/;
        uint32_t lsb = buffer[offset+1] & 63 /*last 6 bits*/;
        uint32_t codepoint = (msb << 6) | lsb;
        offset += 2;
        return codepoint;
    } else if (buffer[offset] >> 4 == 14 /*1110xxxx*/) {
        // A three-byte sequence
        uint32_t msb = buffer[offset] & 15 /*last 4 bits*/;
        uint32_t smsb = buffer[offset+1] & 63;
        uint32_t lsb = buffer[offset+2] & 63;
        offset += 3;
        return (msb << 12) | (smsb << 6) | lsb;
    } else if (buffer[offset] >> 3 == 30 /*11110xxx*/) {
        // A four-byte sequence
        uint32_t msb = buffer[offset] & 7; /*last 3 bits*/
        uint32_t smsb = buffer[offset+1] & 63;
        uint32_t slsb = buffer[offset+2] & 63;
        uint32_t lsb = buffer[offset+3] & 63;
        offset += 4;
        return (msb << 18) | (smsb << 12) | (slsb << 6) | lsb;
    } else {
        throw std::domain_error
            ("buffer does not hold a valid UTF-8 character.");
    }
}

inline std::basic_string<uint32_t> utf8_to_unicode(std::string const &str) {
    std::string::size_type i = 0;
    std::basic_string<uint32_t> codepoints;

    while (i < str.length()) {
        codepoints.push_back(utf8char_to_unicode(str.data(), i));
    }

    return codepoints;
}

inline uint32_t get_unicode_from_utf8(std::istream *input_stream_p) {
    char buffer[4];
    size_t offset = 0;
    input_stream_p->get(buffer[0]);
    if (input_stream_p->gcount() == 0)
        return 0;
    if (buffer[0] >= 192) // 11xxxxxx => at least two bytes
        input_stream_p->get(buffer[1]);
    if (buffer[0] >= 224) // 111xxxxx => at least three bytes
        input_stream_p->get(buffer[2]);
    if (buffer[0] >= 240) // 1111xxxx => four bytes
        input_stream_p->get(buffer[3]);
    return utf8char_to_unicode(buffer, offset);
}

}
#endif

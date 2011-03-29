#include <string>
#include <cstdint>
#include <stdexcept>

#include "utils.hpp"

namespace trtok {

std::basic_string<uint32_t> utf8_to_unicode(std::string const &str) {
	std::string::size_type i = 0;
	std::basic_string<uint32_t> codepoints;

	while (i < str.length()) {
		if (str[i] >> 7 == 0 /*0xxxxxxx*/) {
			// An ASCII singleton.
			codepoints.push_back(str[i]);
			i++;
		} else if (str[i] >> 5 == 6 /*110xxxxx*/) {
			// A two-byte sequence
			if (str.length() - i < 2) {
				throw std::domain_error("str is not a valid UTF-8 string");
			}
			uint32_t msb = str[i] & 31 /*last 5 bits*/;
			uint32_t lsb = str[i+1] & 63 /*last 6 bits*/;
			codepoints.push_back((msb << 8) | lsb);
			i += 2;
		} else if (str[i] >> 4 == 14 /*1110xxxx*/) {
			// A three-byte sequence
			if (str.length() - i < 3) {
				throw std::domain_error("str is not a valid UTF-8 string");
			}
			uint32_t msb = str[i] & 15 /*last 4 bits*/;
			codepoints.push_back((msb << 16) | ((str[i+1] & 63) << 8) | (str[i+2] & 63));
			i += 3;
		} else if (str[i] >> 3 == 30 /*11110xxx*/) {
			// A four-byte sequence
			if (str.length() - i < 4) {
				throw std::domain_error("str is not a valid UTF-8 string");
			}
			uint32_t msb = str[i] & 7; /*last 3 bits*/
			codepoints.push_back((msb << 24) | ((str[i+1] & 63) << 16) | ((str[i+2] & 63) << 8) | (str[i+3] & 63));
			i += 4;
		} else {
			throw std::domain_error("str is not a valid UTF-8 string");
		}
	}

	return codepoints;
}

}

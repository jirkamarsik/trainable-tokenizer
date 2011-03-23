#ifndef UTILS_INCLUDE_GUARD
#define UTILS_INCLUDE_GUARD

namespace trtok {

inline bool is_whitespace(QUEX_TYPE_CHARACTER c)
{
	return ((c >= 0x0009) && (c <= 0x000D)) || (c == 0x0020) || (c == 0x0085) || (c == 0x00A0)
	     || (c == 0x1680) || (c == 0x180E) || ((c >= 0x2000) && c <= (0x200A))
	    || ((c >= 0x2028) && (c <= 0x2029)) || (c == 0x202F) || (c == 0x205F) || (c == 0x3000);
}

}
#endif

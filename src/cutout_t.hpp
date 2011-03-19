#ifndef CUTOUT_T_INCLUDE_GUARD
#define CUTOUT_T_INCLUDE_GUARD

#include <string>

enum cutout_kind_t {
	ENTITY,
	XML
};

/* The type representing a change done by the text cleaner, which
 * can either be removed XML element or an expanded entity. These
 * are sent from the text cleaner to the output formatter which undos
 * these changes. */
struct cutout_t {
	// Have we expanded an entity or have we removed XML markup?
	cutout_kind_t kind;
	// How many nonblank characters were there before the cutout?
	int position;
	// What entity have we rewritten or what XML markup have we removed?
	std::string text;
};

#endif

#ifndef CUTOUT_T_INCLUDE_GUARD
#define CUTOUT_T_INCLUDE_GUARD

#include <string>

enum cutout_type_t {
	ENTITY_CUTOUT,
	XML_CUTOUT,
	SYNC_MARK
};

/* The type representing a change done by the text cleaner, which
 * can either be removed XML element or an expanded entity. These
 * are sent from the text cleaner to the output formatter which undos
 * these changes. This type can also hold a special value which
 * doesn't represent cutout but is used to tell the output formatter
 * that there will be no cutouts till the specified position. */
struct cutout_t {
	// Have we expanded an entity, or removed XML markup
	// or are we simply telling the formatter it can write on.
	cutout_type_t type;
	// How many nonblank characters were there before the cutout in the
	// case of *_CUTOUT. In case of SYNC_MARK, it is the next position
	// till which we can guarantee there will be no cutouts.
	long position;
	// What entity have we rewritten or what XML markup have we removed?
	std::string text;
};

#endif

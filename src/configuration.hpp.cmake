#ifndef CONFIGURATION_INCLUDE_GUARD
#define CONFIGURATION_INCLUDE_GUARD

#define CHUNK_SIZE @CHUNK_SIZE@
#define WORK_UNIT_COUNT @WORK_UNIT_COUNT@
#define ACCUMULATOR_CAPACITY @ACCUMULATOR_CAPACITY@
#define CMAKE_COMMAND "@CMAKE_COMMAND@"
#cmakedefine USE_ICONV
#cmakedefine USE_ICU

#endif

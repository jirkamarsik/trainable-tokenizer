# A minimal Makefile which will serve as the basis for the general CMake
# listfile.
#
# PURPOSE: Makefile Demo Application of Quex
#
# ABSOLUTELY NO WARRANTY
#_______________________________________________________________________________
.PHONY: clean

ifndef QUEX_PATH
    $(error The environment variable QUEX_PATH is not defined!)
endif

# (*) COMPILER SETTINGS ________________________________________________________
#     (change COMPILER to whatever you use as compiler on the command line,
#      e.g. "make COMPILER=icpc" will use intel's c++ compiler)
COMPILER = g++ #-std=c++0x # -ggdb 

CC = $(COMPILER) \
	 -I./ -I$(QUEX_PATH)                             \
	 -DQUEX_OPTION_NO_COMPUTED_GOTOS                 \
	 -DQUEX_OPTION_ASSERTS_DISABLED                 \
	
# (*) RULES ____________________________________________________________________
# -- application
lexer:          main.cpp RoughLexer.cpp
	$(CC) -o $@ main.cpp RoughLexer.cpp 
          
RoughLexer.cpp: RoughTok.qx
	quex -i                 RoughTok.qx	\
	     --engine           RoughLexer	\
	     -b 4				\
	     --iconv				\
	     --token-id-offset	10000

# (*) HELPERS __________________________________________________________________
clean:	
	rm -f RoughLexer*
	rm -f *.o
	rm -f lexer

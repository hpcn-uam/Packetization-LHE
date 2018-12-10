#
# Eduardo Miravalls Sierra
#
# This makefile scans $(SRCDIR) for .c files and compiles them, generating $(EXE).
#

.PHONY: clean depclean all help mkdirs debug release docs debug_time timing tests add_first_block_id.out

.PRECIOUS: %.o

# set the shell for the commands
SHELL := /bin/bash

# Select compiler and linker
# colorgcc if available, gcc otherwise.
ifeq "$(shell command -v colorgcc >/dev/null; echo $$?)" "0"
CC             :=colorgcc
LD             :=colorgcc
else
CC             :=gcc
LD             :=gcc
endif

# object and dependency files
OBJDIR         :=obj
# main source directory
SRCDIR         :=src
# include directories
INCDIR         :=$(SRCDIR) include
# include files
INCLUDE        :=$(addprefix -I,$(INCDIR))

# check if version.h available
ifeq "$(shell ls include/version.h &>/dev/null; echo $$?)" "0"
VERSION_AVAILABLE:=1
else
VERSION_AVAILABLE:=0
endif

# Compiler flags
CFLAGS          =-Wall -Wextra $(INCLUDE) -std=c99 -D_GNU_SOURCE -DVERSION_AVAILABLE=$(VERSION_AVAILABLE) $(CEXTRAFLAGS) -DNOT_FIRST_BLOCK_ID
# Linker flags
LDFLAGS         =
# Libraries
LIBS           :=-lpcap -lm -lpthread
# source files
SRC            :=$(shell find $(SRCDIR) -type f -name '*.c' | grep -v 'test')
# object files
OBJ            :=$(addprefix $(OBJDIR)/,$(SRC:.c=.o))
# header dependencies of source files.
DEP_FILES      :=$(OBJ:.o=.d)
# the executable to generate
NAME           :=LHEPacketizer
EXE            :=$(NAME).out

all:	mkdirs $(EXE)

$(EXE):	$(OBJ)
	@echo -e "#----------------------------------------"
	@echo -e "#Linking $@:"
	$(LD) $(LDFLAGS) $^ -o $@ $(LIBS)
	@echo -e "DONE\n"

#directories that this Makefile needs that exist in order to work.
mkdirs:
	@mkdir -p $(OBJDIR)

#clean methods
distclean:
	rm -f core $(EXE) $(NAME).opt $(NAME).debug add_first_block_id.out
	find $(OBJDIR) -type f -delete

depclean:
	rm -f $(DEP_FILES)

clean:
	find $(OBJDIR) -type f -delete

help:
	@echo -e "make [arg]"
	@echo -e "\tall - por defecto.  Compila el ejecutable '$(EXE)'"
	@echo -e "\tclean -borra los ficheros objeto."
	@echo -e "\tdepclean - borra los ficheros de dependencias."
	@echo -e "\tdistclean - borra todos los ficheros que puede generar este Makefile."
	@echo -e "\tdebug - compila en modo depuración."
	@echo -e "\trelease - compila con optimizaciones."
	@echo -e "\tdocs - compila la documentación."
	@echo -e "\ttests - compila y ejecuta los tests"
	@echo -e "\tadd_first_block_id - compila el programa add_first_block_id.out"


# This is a workaround / hack: call it however you please
# If include/version.h doesn't exist, gcc breaks when generating main.d
# so main.c doesn't depend on include/version.h, so it doesn't get generated
# Finally, compilation fails on main.o because it *does* depend on include/version.h
ifneq "$(shell ls include/version.h 2>/dev/null; echo $$?)" "0"
$(OBJDIR)/src/LHEpack.d:	include/version.h
endif

# Rule to genenerate dependency files
$(OBJDIR)/%.d: %.c
	@mkdir -p $(@D)
	@echo -e "#----------------------------------------"
	@echo -e "#Generating dependency file $@:"
	$(CC) $(CFLAGS) -I$(dir $<) -MM -MP -c $< -MT $(@:.d=.o) > $@


debug:
	$(MAKE) -e OBJDIR=obj/debug CFLAGS+="-Wall -Wextra $(INCLUDE) -std=c99 -D_GNU_SOURCE $(CEXTRAFLAGS) -g -DDEBUG" EXE=$(NAME).debug

release:
	$(MAKE) -e OBJDIR=obj/release CFLAGS+="-Wall -Wextra $(INCLUDE) -std=c99 -D_GNU_SOURCE $(CEXTRAFLAGS) -O3 -DNDEBUG -DDEBUG=0" EXE=$(NAME).opt

timing: CFLAGS+="-DTIMING"
timing:	all

bundle: $(EXE)
	@mkdir -p "detect_retx"
	@cp -r Makefile Doxyfile src/ doc/ srclib/ include/ "detect_retx"
	@tar -cvzf detect.tar.gz "detect_retx"
	@rm -rf "detect_retx"

# General pattern rules
$(OBJDIR)/%.o:	%.c
	@mkdir -p $(@D)
	@echo -e "#----------------------------------------"
	@echo -e "#Building $@ which depends on $^:"
	$(CC) $(CFLAGS) -I$(dir $<) -c $< -o $@

docs:
	doxygen

%.c:

%.h:

%.d:


tests:
	$(CC) $(CFLAGS) $(INCLUDE) src/utils.c tests/test_RTP.c -o test_RTP.out
	valgrind ./test_RTP.out

add_first_block_id.out:
	$(CC) $(CFLAGS) $(INCLUDE) -DNOT_FIRST_BLOCK_ID src/LHE_reader.c src/utils.c src/Buffer.c util/add_first_block_id.c -o $@

ifeq "$(shell git status &>/dev/null ; echo $$?)" "0"
# version commits
VERSION        :=$(shell git describe --always --abbrev=10 --tags)
DIRTY_VERSION  :=$(shell git describe --always --abbrev=10 --tags --dirty)
GIT_ROOT       :=$(shell git rev-parse --show-cdup)

include/version.h:	$(GIT_ROOT).git/HEAD $(GIT_ROOT).git/index Makefile $(shell find . -type f \( -name '*.c' -o -name '*.h' \) | grep -v version.h)
	@echo -e "#----------------------------------------"
	@echo -e "#Generating include/version.h:"
	@echo -e "#New version is: $(DIRTY_VERSION)"
	@echo -e '/**' > "include/version.h"
	@echo -e ' * @file' >> "include/version.h"
	@echo -e ' * @author: Eduardo Miravalls Sierra' >> "include/version.h"
	@echo -e ' * @author: David Muelas Recuenco' >> "include/version.h"
	@echo -e ' * @date:  ' $(shell date --rfc-3339='seconds') >> "include/version.h"
	@echo -e ' */' >> "include/version.h"
	@echo -e '' >> "include/version.h"
	@echo -e '' >> "include/version.h"
	@echo -e '#ifndef __GIT_VERSION_H__' >> "include/version.h"
	@echo -e '#define __GIT_VERSION_H__' >> "include/version.h"
	@echo -e '' >> "include/version.h"
	@echo -e '#ifdef  __cplusplus' >> "include/version.h"
	@echo -e 'extern "C" {' >> "include/version.h"
	@echo -e '#endif' >> "include/version.h"
	@echo -e '' >> "include/version.h"
	@echo -e 'const char COMMIT[] = "$(DIRTY_VERSION)";' >> "include/version.h"
	@echo -e 'const char BUILD_TIMESTAMP[] = "'$(shell date --rfc-3339='seconds')'";' >> "include/version.h"
	@echo -e '' >> "include/version.h"
	@echo -e '#ifdef  __cplusplus' >> "include/version.h"
	@echo -e '}' >> "include/version.h"
	@echo -e '#endif' >> "include/version.h"
	@echo -e '' >> "include/version.h"
	@echo -e '#endif /* __GIT_VERSION_H__ */' >> "include/version.h"
	@echo -e '' >> "include/version.h"
	@echo -e '' >> "include/version.h"
endif

# load autogenerated header dependencies
-include $(DEP_FILES)

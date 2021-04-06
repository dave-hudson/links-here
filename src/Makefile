#
# By default we want to "make all".
#
.DEFAULT_GOAL := all

#
# Common commands and flags.
#
AR := ar
ARFLAGS = rc

CCPP := g++
CPPFLAGS = \
    -fPIC \
    -std=c++14 \
    -fno-omit-frame-pointer \
    -g \
    -O2 \
    -Werror \
    -Wall \
    -Wtype-limits \
    -Wconversion

INSTALL := install
INSTALLFLAGS =

LD := g++
LDFLAGS = $(LDFLAGS_OS) $(LDFLAGS_EXTRA)

MAKE := make

RM := rm

#
# Source files.
#
SRCS := links-here.cpp

#
# Create a list of object files from source files.
#
OBJS := $(patsubst %.cpp,%.o,$(filter %.cpp,$(SRCS)))

#
# App we want to build.
#
TARGET_BIN := links-here

#
# all rule.
#
.PHONY: all

all: $(TARGET_BIN)

$(TARGET_BIN): $(OBJS)
	$(LD) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)

-include $(OBJS:.o=.d)

%.o : %.cpp
	$(CCPP) $(CPPFLAGS) -MD -c $< -o $@

#
# Common rules
#
.PHONY: clean

clean:
	$(RM) -f $(TARGET_BIN)
	$(RM) -f *.ii *.s *.o *.d

.PHONY: realclean

realclean: clean
	$(RM) -f *~
	$(RM) -f .*~


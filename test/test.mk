SRCS+=test.cpp
SRCS+=GCode.cpp

LIB_AREA=${SRCDIR}/../obj.Darwin/libarea.a
BUILD_DEPENDS += ${LIB_AREA}
LDFLAGS += ${LIB_AREA}

include sw.prog.mk

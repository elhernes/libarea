
INCLUDES+=${SRCDIR}

SRCS+=Arc.cpp Area.cpp AreaDxf.cpp AreaOrderer.cpp AreaPocket.cpp Circle.cpp
SRCS+= Curve.cpp clipper.cpp dxf.cpp

SRCS+= AreaClipper.cpp

SRCS+=kurve/Construction.cpp kurve/Finite.cpp kurve/Matrix.cpp kurve/kurve.cpp kurve/offset.cpp

#SRCS+= AreaBoolean.cpp
#SRCS+=kbool/src/booleng.cpp kbool/src/graph.cpp kbool/src/graphlst.cpp kbool/src/instonly.cpp kbool/src/line.cpp kbool/src/link.cpp kbool/src/lpoint.cpp kbool/src/node.cpp kbool/src/record.cpp kbool/src/scanbeam.cpp

#SRCS+= test.cpp

include sw.lib.mk


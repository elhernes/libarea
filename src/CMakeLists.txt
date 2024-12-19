cmake_minimum_required (VERSION 3.24)

project (area)

set (CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(area STATIC
Arc.cpp
Arc.h
Area.cpp
Area.h
AreaClipper.cpp
AreaDxf.cpp
AreaDxf.h
AreaOrderer.cpp
AreaOrderer.h
AreaPocket.cpp
Box2D.h
Circle.cpp
Circle.h
Curve.cpp
Curve.h
Point.h
clipper.cpp
dxf.cpp
dxf.h
kurve/Construction.cpp
kurve/Finite.cpp
kurve/Matrix.cpp
kurve/geometry.h
kurve/kurve.cpp
kurve/offset.cpp
)

find_package(Qt4)

include(${QT_USE_FILE})

QT4_WRAP_CPP(QT_MOC_SOURCES widget.hpp OPTIONS -DBOOST_TT_HAS_OPERATOR_HPP_INCLUDED)
include_directories(./)

set(qt_sources
  qt.cpp
  widget.cpp
  #      hpx_qt.cpp
  ${QT_MOC_SOURCES})

include_directories(QT4_INCLUDE_DIRS})

add_hpx_executable(qt
  MODULE qt
  SOURCES ${qt_sources}
  ${qt_FLAGS}
  FOLDER "Examples/Qt")

target_link_libraries(qt_exe ${QT_LIBRARIES})

#target_link_libraries(qt_exe ${HPX_LIBS}) 

set (proj Anima-Private)

set (cmake_args
  -DCMAKE_C_FLAGS:STRING=${cmake_flags}
  -DCMAKE_CXX_FLAGS:STRING=${cmake_flags}
  -DBUILD_SHARED_LIBS:BOOL=${BUILD_SHARED_LIBS}
  -DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}
  -DANIMA_DIR:PATH=${Anima_BUILD_DIR}
  -DEXECUTABLE_OUTPUT_PATH=${CMAKE_BINARY_DIR}/bin
  -DLIBRARY_OUTPUT_PATH=${CMAKE_BINARY_DIR}/lib
  -DBUILD_TESTING:BOOL=${BUILD_ANIMA_TESTING}
  )

set (location "")
if (NOT DEFINED ${proj}_SRC_DIR)
  set(location GIT_REPOSITORY ${GITHUB_PREFIX}ocommowi/Anima.git GIT_TAG anima-private)
endif()

ExternalProject_Add(${proj}
  ${location}
  DEPENDS Anima
  PREFIX ${CMAKE_BINARY_DIR}/${proj}
  SOURCE_DIR ${CMAKE_SOURCE_DIR}/${proj}
  CMAKE_ARGS ${cmake_args}
  BUILD_IN_SOURCE 0
  BINARY_DIR ${CMAKE_BINARY_DIR}/${proj}
  UPDATE_COMMAND ""
  INSTALL_COMMAND ""
  )

cmake_minimum_required(VERSION 3.15.0)

list(APPEND EXTRA_COMPONENT_DIRS ${CMAKE_CURRENT_LIST_DIR}/rainmaker/components)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp-app-template)

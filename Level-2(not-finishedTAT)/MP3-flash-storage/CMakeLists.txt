# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.22)

# Set extra component directories
set(EXTRA_COMPONENT_DIRS components)

# Include project.cmake from IDF
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Set partition table
set(PARTITION_TABLE_CUSTOM_PARTITION_FILENAME "partitions.csv")
set(PARTITION_TABLE_CUSTOM_APP_FILENAME "partitions.csv")

# Project name
project(MP3-flash-storage)

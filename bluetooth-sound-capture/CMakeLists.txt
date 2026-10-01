# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

# Prevent show strict checking KConfig (In this file Kconfig.in has a BUG in 
# BTDM_CTRL_LEGACY_AUTH_VENDOR_EVT_EFF param definition =), so we are just
# suppressing that to make build log output more cleanly)
set(ENV{INT_KCONFIG_STRICT} "n")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(app)

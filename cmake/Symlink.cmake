# Symbol link module for CMake
#
# Usage:
#   symlink(<DEST> <SOURCE>)
#   install_symlink(<DEST> <SOURCE>)

macro(symlink _dest _source)
  set(_ln_failed)
  message(STATUS "Installing link: ${_source} -> ${_dest}")
  execute_process(
    COMMAND ln -sf ${_dest} ${_source}
    RESULT_VARIABLE _ln_failed)
  if(_ln_failed)
    message(FATAL_ERROR "ln failed")
  endif(_ln_failed)
endmacro(symlink)

macro(install_symlink _dest _source)
  install(CODE "include(\"${CMAKE_HOME_DIRECTORY}/cmake/Symlink.cmake\")
                symlink(\"${_dest}\" \"\$ENV{DESTDIR}${_source}\")")
endmacro(install_symlink)

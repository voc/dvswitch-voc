// Copyright 2007 Ben Hutchings <ben@decadent.org.uk>.
// See the file "COPYING" for licence details.

#ifndef INC_AUTO_PIPE_HPP
#define INC_AUTO_PIPE_HPP

#include "auto_fd.hpp"

struct auto_pipe
{
    explicit auto_pipe(int reader_flags = 0, int writer_flags = 0);
    auto_fd reader, writer;
};

#endif // !defined(INC_AUTO_PIPE_HPP)

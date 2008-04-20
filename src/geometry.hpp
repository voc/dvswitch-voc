// Copyright 2005-8 Ben Hutchings <ben@decadent.org.uk>.
// See the file "COPYING" for licence details.

#ifndef DVSWITCH_GEOMETRY_HPP
#define DVSWITCH_GEOMETRY_HPP

#include <algorithm>

struct rectangle
{
    int left, top;     // inclusive
    int right, bottom; // exclusive

    rectangle operator|=(const rectangle & other)
    {
	if (other.empty())
	{
	    // use current extents unchanged
	}
	else if (empty())
	{
	    // use other extents
	    *this = other;
	}
	else
	{
	    // find rectangle enclosing both extents
	    left = std::min(left, other.left);
	    top = std::min(top, other.top);
	    right = std::max(right, other.right);
	    bottom = std::max(bottom, other.bottom);
	}

	return *this;
    }

    rectangle operator&=(const rectangle & other)
    {
	// find rectangle enclosed in both extents
	left = std::max(left, other.left);
	top = std::max(top, other.top);
	right = std::max(left, std::min(right, other.right));
	bottom = std::max(top, std::min(bottom, other.bottom));
	return *this;
    }

    bool empty() const
    {
	return left == right || bottom == top;
    }
};

#endif // !DVSWITCH_GEOMETRY_HPP

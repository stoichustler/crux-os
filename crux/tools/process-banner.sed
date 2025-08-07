#!/bin/sed -rf
# Process a text input, to turn it into a C string for the CRUX_BANNER macro.

# Strip trailing whitespace.
s_ *$__

# Escape backslashes.
s_\\_\\\\_g

# Enclose the line in "...\n".
s_(.*)_"\1\\n"_

# Trailing \ on all but the final line.
$!s_$_ \\_

# Append closing header guard
$a\
\
#endif /* CRUX_COMPILE_H */

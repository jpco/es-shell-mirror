#!/usr/local/bin/es --

test = @ {eval $*; result ok}

# we just want to not crash
skip case 'let (x) {x = {}; echo $x}'
# want ok

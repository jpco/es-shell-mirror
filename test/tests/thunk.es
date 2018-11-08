#!/usr/local/bin/es --

# Simply verify that es correctly deduplicates nested blocks of code.

test = eval result

case '{}'; want '{}'
case '{{}}'; want '{}'
case '{{{}}}'; want '{}'
case '{{{{}}}}'; want '{}'
case '{{{{{}}}}}'; want '{}'

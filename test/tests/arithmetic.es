#!/usr/local/bin/es --

# Tests that arithmetic expressions result in the correct value, &
# that printing/reading them produces the same value.

test = @ expr {
  let (
    res = <={get-output 'echo `('$expr')'}
    cmd = <={x = `` \n {eval echo '{echo `('$expr')}' >[2] /dev/null}; result $x}
  ) {
    let (reparsed = <={get-output $cmd})
    if {~ $res *'syntax error'} {
      result $res
    } {!~ $res $reparsed} {
      result ''''^$^res^'''' does not match ''''^$^reparsed^''''
    } {
      result $res
    }
  }
}

case '3 + 1'        ; want 4
case '3 - 2.5'      ; want 0.5
case '-(3 - 1)'     ; want -2
case '-(3 + 1)'     ; want -4
case '-3 - 1'       ; want -4
case '-0'           ; want 0

# precedence
case '-(3-3) != (-3)-3'   ; want true

# error handling
case '3.0 % 2.0'    ; want 1.0
case '1.5 % 2'      ; want 'error code 1: left-hand side of % is not int-valued'
case '3 / 0'        ; want 'error code 1: divide by zero'

# (in)equality tests
case '1.5 < 3.5'    ; want true
case '3 < 3'        ; want false
case '3 < 0.5 + 2'  ; want false
case '3.5 < 1'      ; want false

case '3.5 >= 1.5'   ; want true
case '3 >= 3'       ; want true
case '3 >= 3.0'     ; want true
case '1.5 >= 3'     ; want false

case '3.5 = 1.5'    ; want false
case '3 == 3'       ; want true
case '3 = 3.0'      ; want true
case '1.5 == 3'     ; want false

case '1 != 4'       ; want true
case '-1.0 != 3-4'  ; want false

# logical operators and (in)equality chaining
case '1 < 2 < 3'      ; want true
case '1 < 2 > 1'      ; want true
case '1 < 2 = 2 < 0'  ; want false

case '1 < 2 && 3 < 2' ; want false
case '1 > 2 && 3 > 2' ; want false
case '1 < 2 || 3 < 2' ; want true
case '3 < 2 || 1 < 2' ; want true

#!/usr/local/bin/es

fn with-tmpfile file rest {
  local ($file = `mktemp)
  unwind-protect {
    if {~ $#rest 1} {$rest} {with-tmpfile $rest}
  } {
    rm -f $$file
  }
}

test = @ fl * {
  with-tmpfile tmpf {
    echo $* > $tmpf
    if {~ $fl ''} {
      $&backquote \n {. $tmpf >[2=1]}
    } {
      $&backquote \n {. $fl $tmpf >[2=1]}
    }
  }
}

case -e  true                          ; want 0
case -e  catch {echo false} {result 1} ; want 0 'false'
case -xe true                          ; want 0 '{true}'
case -x  result 1                      ; want 1 '{result 1}'
case -v  result 2                      ; want 2 'result 2'
case -nx result 3                      ; want 0 '{result 3}'

case '' '
runflags = noexec
result 2
'
want 0

case '' '
runflags = noexec echoinput
result 2
'
want 0 'result 2'

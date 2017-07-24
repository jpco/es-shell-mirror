#! /usr/local/bin/es --

fn t cmd-string want {
  let (res = ()
       got = ()) {
    # awful, but a requirement for test cases which may cause syntax errors.
    if {!{res = <={subshell {eval $cmd-string} > /tmp/es-test >[2] /tmp/es-err}}} {
      let (err-msg = ': '^`` \n {cat /tmp/es-err})
        echo '[31mfailed[0m:' $cmd-string '(error '^$^res^$^err-msg^')'
      return
    } {
      got = `{cat /tmp/es-test}
    }
    if {~ $^want $^got} {
      echo 'passed:' $cmd-string
    } {
      echo '[31mfailed[0m:' $cmd-string
      echo ' - want' $want
      echo ' - got' $got
    }
  }
}

t 'a=b; echo $a' 'b'
t 'echo a=b' 'a=b'
t 'echo a=b; a=b; echo $a' 'a=b b'
t 'let (a=b) echo $a' 'b'
t 'let (a=b;c=d) echo $a, $c' 'b, d'
t 'let (a=b) c=$a; echo $c' 'b'
t 'echo (a=a a=b)' 'a=a a=b'
t 'echo (a b) a=b' 'a b a=b'
t 'echo a=b & a=c; echo $a' 'c a=b'
t 'a=b || a=c; echo $a' 'c'
t 'if {~ a a} {a=b; echo $a}' 'b'
t '(a b)=(c=d d=e); echo $b $a' 'd=e c=d'
t 'let ((a b)=(1=2 2=3 3=4 4=5)) echo $b' '2=3 3=4 4=5'
t '!a=b c; echo $a' 'b c'
t 'if {~ a=b a=b} {echo yes}' 'yes'
t 'let (a=b=c=d) echo $a' 'b=c=d'
t '''a=b'' = c=d; echo $''a=b''' 'c=d'
t 'let (''a=b''=c=d) echo $''a=b''' 'c=d'
t 'a =b= c==d= e=f; echo $a' 'b= c==d= e=f'
t 'a=b = c = d; echo $a' 'b = c = d'

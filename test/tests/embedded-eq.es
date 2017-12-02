#! /usr/local/bin/es --

test = @ cmd {
  with-tmpfile out err {
    let (res = <={fork {eval $cmd} > $out >[2] $err}) {
      if {!~ $res 0} {
        let (err-msg = ': '^`` \n {cat $err})
          result 'error code '^$^res^$^err-msg
      } {
        let (out-msg = `` \n {cat $out})
          result $^out-msg
      }
    }
  }
}

case 'a=b; echo $a'
want 'b'

case 'echo a=b'
want 'a=b'

case 'echo a=b; a=b; echo $a'
want 'a=b b'

case 'let (a=b) echo $a'
want 'b'

case 'let (a=b;c=d) echo $a, $c'
want 'b, d'

case 'let (a=b) c=$a; echo $c'
want 'b'

case 'echo (a=a a=b)'
want 'a=a a=b'

case 'echo (a b) a=b'
want 'a b a=b'

case 'echo a=b & a=c; echo $a'
want 'c a=b'

case 'a=b || a=c; echo $a'
want 'c'

case 'if {~ a a} {a=b; echo $a}'
want 'b'

case '(a b)=(c=d d=e); echo $b $a'
want 'd=e c=d'

case 'let ((a b)=(1=2 2=3 3=4 4=5)) echo $b'
want '2=3 3=4 4=5'

case '!a=b c; echo $a'
want 'b c'

case 'if {~ a=b a=b} {echo yes}'
want 'yes'

case 'let (a=b=c=d) echo $a'
want 'b=c=d'

case '''a=b'' = c=d; echo $''a=b'''
want 'c=d'

case 'let (''a=b''=c=d) echo $''a=b'''
want 'c=d'

case 'a =b= c==d= e=f; echo $a'
want 'b= c==d= e=f'

case 'a=b = c = d; echo $a'
want 'b = c = d'

case 'echo a = b'
want 'a = b'

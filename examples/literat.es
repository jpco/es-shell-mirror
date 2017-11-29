#!/usr/local/bin/es --

let (
  cmdbuf
  fn get-input {if {~ <={%read </dev/tty} 'q'} {break}}
)
cat $* | for-each @ line {
  if {~ $line ''} {
    if {!~ $cmdbuf ()} {
      for (cmd = $cmdbuf) eval $cmd
      cmdbuf =
    }
    get-input
  } {
    echo $line
    if {~ $line ';'^*} {
      cmdbuf = $^cmdbuf^$line
    }
  }
}

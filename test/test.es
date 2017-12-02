#!/usr/local/bin/es --

# TODO: allow >1 arg for cases, >1 arg for want
fn run-file file {
  echo $file
  local (failed = 0; test; cases; wants;
    fn-case = @ args {cases = $cases {result $args}}
    fn-want = @ want {wants = $wants {result $want}}
  ) {
    . $file
    if {!~ $#cases $#wants} {
      throw error run-file 'mismatch between number of cases and number of desired results'
    }

    for (pa = $cases; pw = $wants) let (args = <=$pa; want = <=$pw) {
      let (got = <={$test $args}) if {~ $got $want} {
        echo ' - PASSED:' $args
      } {
        failed = `($failed + 1)
        echo ' - [31mFAILED[0m:' $args
        echo '   got:' $got
        echo '   want:' $want
      }
    }
    result $failed
  }
}

let (failed) {
  for (f = ./tests/*.es) {
    if {!~ <={fork {run-file $f}} 0} {
      failed = $failed $f
    }
    echo
  }
  echo ' ============== '\n
  echo 'Failed tests:' <={%flatten ', ' $failed}
}

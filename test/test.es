#!/usr/local/bin/es --

# helper functions

fn with-tmpfile file rest {
  local ($file = `mktemp)
  unwind-protect {
    if {~ $#rest 1} {$rest} {with-tmpfile $rest}
  } {
    rm -f $$file
  }
}

fn get-output cmd {
  with-tmpfile out err {
    let (res = <={fork {eval $cmd} > $out >[2] $err}) {
      if {!~ $res 0} {
        let (err-msg = `` \n {cat $err})
          result 'error code '^$^res^': '^$^err-msg
      } {
        let (out-msg = `` \n {cat $out})
          result $^out-msg
      }
    }
  }
}

# driver functions
fn run-file file {
  echo $file
  local (
    passed = 0; failed = 0; test; cases; wants;

    fn-case = @ args {cases = $cases {result $args}}
    fn-want = @ want {wants = $wants {result $want}}
  ) {
    . $file
    if {!~ $#cases $#wants} {
      throw error run-file 'mismatch between number of cases and number of desired results'
    }

    for (pa = $cases; pw = $wants) let (args = <=$pa; want = <=$pw) {
      let (got = <={$test $args})
	if @ {for (a = $got; b = $want) if {!~ $a $b} {return 1}} {
	  passed = `($passed + 1)
	} {
	  failed = `($failed + 1)
	  echo ' - [31mFAILED[0m:' $args
	  echo '   got:' $got
	  echo '   want:' $want
	}
    }
    echo ' -' $passed cases passed
    result $failed
  }
}

fn run-tests files {
  let (failed = ()) {
    if {~ $files ()} {files = ./tests/*.es}
    for (f = $files) {
      if {!~ <={fork {run-file $f}} 0} {
        failed = $failed $f
      }
      echo
    }
    if {!~ $#files 1} {echo ' ============== '\n}
    echo 'Failed tests:' <={%flatten ', ' $failed}
  }
}

run-tests $*

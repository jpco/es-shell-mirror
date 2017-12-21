# initial.es -- set up initial interpreter state ($Revision: 1.1.1.1 $)

#
# shellfns -- these get called by the C code
#

# Here there be dragons!
# 
# Fun feature: if arg %foo isn't defined, we automatically define it as $&foo
# Fun feature: the "fn" namespace is only defined here, not in the eval code
shellfn:whatis = @ cmd rest (
  result = ()
) {$&if {~ <={result = $(fn-^$cmd)} ()} {

  if {~ <={result = <={~~ $cmd %*}} ()} {
    # INSERT MORE FUNCTIONALITY HERE
    result ()
  } {
    $&seq {
      fn-^$cmd = '$&'^$result
    } {
      result $(fn-^$cmd)
    }
  }

} {$&result $result}}

shellfn:main = %escaped-by eof @ (result = ()) {
  catch @ e type rest {
    if {~ $e eof} {
      throw eof $result
    } {~ $e exit} {
      throw $e $type $rest
    } {%seq {
      echo caught $e^: $type - $rest
      } {
      throw retry
    }}
  } {
    forever @ (code = <={%parse $prompt}) {
      if {%not {~ <={%count $code} 0}} {
        result = <={$code}
      }
    }
  }
}

#
# some basic convenience functions
#

fn-catch    = $&catch
fn-if       = $&if
fn-result   = $&result
fn-throw    = $&throw

fn-forever  = $&forever

fn-exit = throw exit

fn-whatis = @ * (fn = ()) {
  if {~ <={fn = <={$'shellfn:whatis' $*}} ()} {
    throw error whatis unknown command $*
  } {
    echo $fn
  }
}

fn-eval = @ * {'{' ^ <={%flatten ' ' $*} ^ '}'}

fn-true   = result 0
fn-false  = result 1

#
# Flow control functions && binders
#

fn-%escaped-by = @ ex body {
  catch @ e rest {
    if {~ $e $ex} {
      result $rest
    } {
      throw $e $rest
    }
  } {
    $body
  }
}
fn-escaped-by = @ ex body {%escaped-by $ex local fn-^$ex {$body} throw $ex}

fn-unwind-protect = @ body cleanup (
  exception = (); result = ()
) {
  %seq {
    catch @ e {
      exception = $e
    } {
      result = <=$body
    }
  } {
    $cleanup
  } {
    if {~ $exception ()} {
      result $result
    } {
      throw $exception
    }
  }
}

fn-%while = %escaped-by while-test-is-false @ cond body {
  @ (result = <=true) {
    forever {
      if {%not $cond} {
        throw while-test-is-false $result
      } {
        result = <=$body
      }
    }
  }
}
fn-while = escaped-by break %while

fn-apply = @ cmd args (curr = ()) {
  %while {%not {~ <={(curr args) = $args} ()}} {
    $cmd $curr
  }
}

fn-local = @ var cmd value (
  result = ()
) {
  @ (old = $$var) {
    unwind-protect {
       %seq {
         $var = $value
       } {
         result = <=$cmd
       }
     } {
       %seq {
         $var = $old
       } {
         result $result
       }
     }
  }
}

#
# General hook functions
#

fn-%not = @ cmd {
  if {$cmd} {false} {true}
}

fn-%and = @ first rest {
  @ (result = <={$first}) {
    if {~ <={%count $rest} 0} {
      result $result
    } {result $result} {
      %and $rest
    } {
      result $result
    }
  }
}

fn-%or = @ first rest {
  if {~ <={%count $first} 0} {
    false
  } @ (result = <={$first}) {
    if {~ <={%count $rest} 0} {
      result $result
    } {%not {result $result}} {
      %or $rest
    } {
      result $result
    }
  }
}

fn-%flatten = @ sep result rest {
  if {~ ($result $rest) ()} {
    result ''
  } {
    %seq {
      apply @ i {result = $result^$sep^$i} $rest
    } {
      result $result
    }
  }
}

#
# I/O helpers
#

fn-echo = @ li (
  end = \n
) {
  %seq {
    if {~ $li(1) -n} {
      %seq {
	end = ''
      } {
	li = $li(2 ...)
      }
    } {~ $li(1) --} {
      li = $li(2 ...)
    }
  } {
    $&echo <={%flatten ' ' $li}^$end
  }
}

set-max-eval-depth  = $&setmaxevaldepth
max-eval-depth = 640

ifs   = ' ' \t \n

result es-core initial state

# initial.es -- set up initial interpreter state ($Revision: 1.1.1.1 $)

#
# Builtin functions
#

fn-access	  = $&access
fn-catch	  = $&catch
fn-echo		  = $&echo
fn-exec		  = $&exec
fn-forever	= $&forever
fn-if		    = $&if
fn-result	  = $&result
fn-%split   = $&split
fn-throw	  = $&throw
fn-%read	  = $&read

# Tread carefully.
fn-%whatis = @ cmd rest {
  @ (fn = $(fn-^$cmd)) {
    $&if {~ $fn ()} {
      $&throw error %whatis unknown command $cmd
    } {
      $&result $fn $rest
    }
  }
}

fn-eval = @ * {'{' ^ <={%flatten ' ' $*} ^ '}'}

fn-true		= result 0
fn-false	= result 1

fn-break	= throw break
fn-exit		= throw exit
fn-return	= throw return

fn-catch-return = @ body {
  catch @ e rest {
    if {~ $e return} {
      result $rest
    } {
      throw $e $rest
    }
  } {
    $body
  }
}

fn-unwind-protect = @ body cleanup {
  %seq {
    if {%not {~ <={%count $cleanup} 1}} {
      throw error unwind-protect 'unwind-protect body cleanup'
    }
  } {
    @ (exception) {
      @ (
        result = <={
          catch @ e {
            exception = caught $e
          } {
            $body
          }
        }
      ) {%seq {
        $cleanup
      } {
        if {~ $exception(1) caught} {
          throw $exception(2 ...)
        }
      } {
        result $result
      }}
    }
  }
}

fn-while = @ cond body {
	catch @ e value {
		if {%not {~ $e break}} {
			throw $e $value
		} {
		  result $value
    }
	} {
		@ (result = <=true) {
			forever {
				if {%not $cond} {
					throw break $result
				} {
					result = <=$body
				}
			}
    }
	}
}

fn-apply = @ cmd args {
  @ (res) {%seq {
    while {%not {~ $args ()}} {
      %seq {
        res = <={$cmd $args(1)}
      } {
        args = $args(2 ...)
      }
    }
  } {
    result $res
  }}
}

#
# Syntactic sugar
#

fn-%flatten	= @ sep li {
  @ (res = $li(1)) {%seq {
    apply @ i {res = $res^$sep^$i} $li(2 ...)
  } {
    result $res
  }}
}

fn-%count	= $&count

fn-%seq = $&seq

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
	} {
		@ (result = <={$first}) {
			if {~ <={%count $rest} 0} {
				result $result
			} {%not {result $result}} {
				%or $rest
			} {
				result $result
			}
    }
	}
}

fn-. = @ * {
  local (prompt) {$&dot $*}
}

fn-%parse	= $&parse

fn-%input-loop = catch-return {
	@ (result) {
    catch @ e type msg {
      %seq {
        if {~ $e eof} {
          return $result
        } {~ $e exit} {
          throw $e $type $msg
        } {~ $e error} {
          echo $msg
        } {~ $e signal} {
          if {%not {~ $type sigint sigterm sigquit}} {
            echo caught unexpected signal: $type
          }
        } {
          echo uncaught exception: $e $type $msg
        }
      } {
        throw retry # restart forever loop
      }
    } {
      forever {
        @ (code = <={%parse $prompt}) {
          if {%not {~ <={%count $code} 0}} {
            result = <={$code}
          }
        }
      }
    }
  }
}

#	These functions are potentially passed to a REPL as the %dispatch
#	function.  (For %eval-noprint, note that an empty list prepended
#	to a command just causes the command to be executed.)


#
# Settor functions
#

set-max-eval-depth	= $&setmaxevaldepth

#
# Variables
#

#	These variables are given predefined values so that the interpreter
#	can run without problems even if the environment is not set up
#	correctly.

ifs		= ' ' \t \n
max-eval-depth	= 640

#
# Title
#

#	This is silly and useless, but whatever value is returned here
#	is printed in the header comment in initial.c;  nobody really
#	wants to look at initial.c anyway.

result es-core initial state

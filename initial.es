# initial.es -- set up initial interpreter state ($Revision: 1.1.1.1 $)

#
# The basics
#

fn-catch	  = $&catch
fn-exec		  = $&exec
fn-forever	= $&forever
fn-if		    = $&if
fn-result	  = $&result
fn-%split   = $&split
fn-throw	  = $&throw
fn-%read	  = $&read

# TODO: move this to the appropriate place in the file
fn-echo = @ li (
  end = \n
) {%seq {
  if {~ $li(1) -n} {%seq {
    end = ''
  } {
    li = $li(2 ...)
  }} {~ $li(1) --} {
    li = $li(2 ...)
  }
} {
  $&echo <={%flatten ' ' $li}^$end
}}


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

#
# Flow control helpers
#

fn-%seq = $&seq

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

fn-unwind-protect = @ body cleanup (
  exception = (); result = ()
) {
  %seq {
    if {%not {~ <={%count $cleanup} 1}} {
      throw error unwind-protect 'unwind-protect body cleanup'
    }
  } {
    catch @ e {%seq {
      exception = $e
    } {
      $cleanup
    }} {%seq {
      result = <=$body
    } {
      $cleanup
    }}
  } {
    if {~ $exception ()} {
      result $result
    } {
      throw $exception
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

fn-apply = @ cmd args (
  result = ()
) {
  %seq {
    while {%not {~ $args ()}} {
      %seq {
        result = <={$cmd $args(1)}
      } {
        args = $args(2 ...)
      }
    }
  } {
    result $result
  }
}

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

#
# Syntactic sugar
#

fn-%flatten	= @ sep result rest {
  %seq {
    apply @ i {result = $result^$sep^$i} $rest
  } {
    result $result
  }
}

fn-%count	= $&count

#
# input helpers
#

fn-%parse	= $&parse

fn-%input-loop = catch-return @ (result = ()) {
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
    forever @ (code = <={%parse $prompt}) {
      if {%not {~ <={%count $code} 0}} {
        result = <={$code}
      }
    }
  }
}

set-max-eval-depth	= $&setmaxevaldepth
ifs		= ' ' \t \n
max-eval-depth	= 640

result es-core initial state

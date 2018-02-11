# initial.es -- set up initial interpreter state ($Revision: 1.1.1.1 $)

#
# shell fns -- these get called by the C code
#

# This var, when set, loads up tiny, simple versions of es' shell fns
# (as well as echo). This shrinks up stack traces and makes certain bugs
# much simpler to investigate.
#
ES_MINIMAL = true

$&if {~ $ES_MINIMAL ()} {

  $&seq {

    # Here there be dragons! es:whatis bites back!
    es:whatis = $&keeplexicalbinding @ cmd rest (
      result = ()
    ) {
      $&if {~ <={result = $(fn-^$cmd)} ()} {
	if {~ <={result = <={~~ $cmd %*}} ()} {

	  # XXX: More whatis functionality goes here (e.g., %pathsearch)

	  result ()
	} {
	  $&seq {
	    fn-^$cmd = '$&'^$result
	  } {
	    result $(fn-^$cmd)
	  }
	}
      } {
	$&result $result
      }
    }

  } {

    es:main = %escaped-by eof @ (
      result = ()
    ) {
      catch @ e type rest {
	if {~ $e eof} {
	  throw eof $result
	} {~ $e exit} {
	  throw $e $type $rest
	} {
	  %seq {
	    echo caught $e^: $type - $rest
	  } {
	    throw retry
	  }
	}
      } {
	forever @ (code = <={%parse $prompt}) {
	  if {%not {~ <={%count $code} 0}} {
	    result = <={$code}
	  }
	}
      }
    }

  } {

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

  }

} {

  $&seq {

    es:whatis = $&keeplexicalbinding @ cmd rest {
      $&result $(fn-^$cmd)
    }

  } {

    es:main = forever @ (
      code = <={$&parse $prompt}
    ) {$code}

  } {

    fn-echo = @ li {
      $&echo $li \n
    }

  }

}

#
# some basic convenience functions
#

fn-%seq = $&seq

fn-catch    = $&catch
fn-if       = $&if
fn-result   = $&result
fn-throw    = $&throw

fn-forever  = $&forever

fn-exit = throw exit

# Oh God, I've created a monster
fn-%whatis = $&keeplexicalbinding @ * (fn = ()) {
  if {~ <={fn = <={$'es:whatis' $*}} ()} {
    throw error whatis unknown command $*
  } {
    result $fn
  }
}

fn-whatis = $&keeplexicalbinding @ * (
  result = ()
) {
  if {%not {~ $* ()}} {
    %seq {
      catch @ e f m {
	if {~ $e error} {
	  %seq {
	    echo $m  # >[1=2]
	  } {
	    result = 1
	  }
	} {
	  throw $e $f $m
	}
      } {
	%seq {
	  echo <={%whatis $*(1)}
	} {
	  result = 0
	}
      }
    } {
      result $result <={whatis $*(2 ...)}
    }
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
  exception = ()
  result = ()
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

fn-%runfn = @ nm cmd {
  escaped-by return $cmd
}

fn-fn = @ nm rest {
  @ ((body args) = $rest(<={$&count $rest} ... 1)) {
    %seq {
      if {%not {~ $args ()}} {
	args = $args(<={$&count $args} ... 1)
      }
    } {
      if {~ $body '@'^*} {
	@ (spl = <={$&split '' $body}) {body = <={%flatten '' $spl(2 ...)}}
      }
    } {
      fn-^$nm = %runfn $nm <={eval result '@' $args $body}
    }
  }
}

fn-%while = %escaped-by while-test-is-false @ cond body (
  result = <=true
) {
  forever {
    if {%not $cond} {
      throw while-test-is-false $result
    } {
      result = <=$body
    }
  }
}
fn-while = escaped-by break %while

fn-apply = @ cmd args (
  curr = ()
) {
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
  if {~ $result ()} {
    result ''
  } {
    %seq {
      apply @ i {result = $result^$sep^$i} $rest
    } {
      result $result
    }
  }
}

set-max-eval-depth  = $&setmaxevaldepth
max-eval-depth = 640

ifs   = ' ' \t \n

result es-core initial state

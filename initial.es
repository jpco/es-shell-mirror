# initial.es -- set up initial interpreter state ($Revision: 1.1.1.1 $)

#
# Builtin functions
#

fn-access	  = $&access
fn-break	  = $&break
fn-catch	  = $&catch
fn-echo		  = $&echo
fn-exec		  = $&exec
fn-forever	= $&forever
fn-fork		  = $&fork
fn-if		    = $&if
fn-newpgrp	= $&newpgrp
fn-result	  = $&result
fn-throw	  = $&throw
fn-umask	  = $&umask
fn-wait		  = $&wait
fn-%read	  = $&read

# Tread carefully.
fn-%whatis = @ cmd rest {
  % (fn = $(fn-^$cmd))
    $&if {~ $fn ()} {
      $&throw error %whatis unknown command $cmd
    } {
      $&result $fn $rest
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
    % (exception)
      % (
        result = <={
          catch @ e {
            exception = caught $e
          } {
            $body
          }
        }
      ) %seq {
          $cleanup
        } {
          if {~ $exception(1) caught} {
            throw $exception(2 ...)
          }
        } {
          result $result
        }
  }
}

fn-%fsplit  = $&fsplit
fn-%newfd	  = $&newfd
fn-%split   = $&split

fn-while = @ cond body {
	catch @ e value {
		if {%not {~ $e break}} {
			throw $e $value
		} {
		  result $value
    }
	} {
		% (result = <=true)
			forever {
				if {%not $cond} {
					throw break $result
				} {
					result = <=$body
				}
			}
	}
}

fn-apply = @ cmd args {
  % (res) %seq {
    while {%not {~ $args ()}} {
      %seq {
        res = <={$cmd $args(1)}
      } {
        args = $args(2 ...)
      }
    }
  } {
    result $res
  }
}

#
# Syntactic sugar
#

fn-%flatten	= @ sep li {
  % (res = $li(1)) %seq {
    apply @ i {res = $res^$sep^$i} $li(2 ...)
  } {
    result $res
  }
}

fn-%count	= $&count

fn-%seq = $&seq

fn-%not = @ cmd {
	if {$cmd} {false} {true}
}

fn-%and = @ first rest {
	% (result = <={$first})
		if {~ <={%count $rest} 0} {
			result $result
		} {result $result} {
			%and $rest
		} {
			result $result
		}
}

fn-%or = @ first rest {
	if {~ <={%count $first} 0} {
		false
	} {
		% (result = <={$first})
			if {~ <={%count $rest} 0} {
				result $result
			} {%not {result $result}} {
				%or $rest
			} {
				result $result
			}
	}
}

#	These redirections are rewritten:
#
#		cmd < file		%open 0 file {cmd}
#		cmd > file		%create 1 file {cmd}
#		cmd >[n] file		%create n file {cmd}
#		cmd >> file		%append 1 file {cmd}
#		cmd <> file		%open-write 0 file {cmd}
#		cmd <>> file		%open-append 0 file {cmd}
#		cmd >< file		%open-create 1 file {cmd}
#		cmd >>< file		%open-append 1 file {cmd}
#
#	All the redirection hooks reduce to calls on the %openfile hook
#	function, which interprets an fopen(3)-style mode argument as its
#	first parameter.  The other redirection hooks (e.g., %open and
#	%create) exist so that they can be spoofed independently of %openfile.
#
#	The %one function is used to make sure that exactly one file is
#	used as the argument of a redirection.

fn-%dup = $&dup

fn-%openfile	  = $&openfile
fn-%open	      = %openfile r		# < file
fn-%create	    = %openfile w		# > file
fn-%append	    = %openfile a		# >> file
fn-%open-write	= %openfile r+		# <> file
fn-%open-create	= %openfile w+		# >< file
fn-%open-append	= %openfile a+		# >>< file, <>> file

fn-%one = @ * {
  %seq {
    if {%not {~ <={%count $*} 1}} {
      throw error %one <={
        if {~ <={%count $*} 0} {
          result 'null filename in redirection'
        } {
          result 'too many files in redirection: ' $*
        }
      }
    }
  } {
	  result $*
  }
}

if {~ <=$&primitives readfrom} {
	fn-%readfrom = $&readfrom
} {
	fn-%readfrom = @ var input cmd {
		local ($var = /tmp/es.^$var^.^$pid) {
			unwind-protect {
        %seq {
          %create 1 <={%one $$var} {$input}
        } {
          # text of $cmd is   command file
          $cmd
        }
			} {
				rm -f $$var
			}
		}
	}
}

if {~ <=$&primitives writeto} {
	fn-%writeto = $&writeto
} {
	fn-%writeto = @ var output cmd {
		local ($var = /tmp/es.^$var^.^$pid) {
			unwind-protect {
        %seq {
          %create 1 <={%one $$var} {}
        } {
          $cmd
        } {
          %create 1 <={%one $$var} {$output}
        }
			} {
				rm -f $$var
			}
		}
	}
}

#
# Hook functions
#

#
# Read-eval-print loops
#

fn-. = @ * {
  local (prompt) $&dot $*
}

fn-%parse	= $&parse

fn-%input-loop = catch-return {
	% (result)
    catch @ e type msg {
      %seq {
        if {~ $e eof} {
          return $result
        } {~ $e exit} {
          throw $e $type $msg
        } {~ $e error} {
          %dup 1 2 {echo $msg}
        } {~ $e signal} {
          if {%not {~ $type sigint sigterm sigquit}} {
            %dup 1 2 {echo caught unexpected signal: $type}
          }
        } {
          %dup 1 2 {echo uncaught exception: $e $type $msg}
        }
      } {
        throw retry # restart forever loop
      }
    } {
      forever {
        % (code = <={%parse $prompt})
          if {%not {~ <={%count $code} 0}} {
            result = <={$code}
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

#	Settor functions are called when the appropriately named variable
#	is set, either with assignment or local binding.  The argument to
#	the settor function is the assigned value, and $0 is the name of
#	the variable.  The return value of a settor function is used as
#	the new value of the variable.  (Most settor functions just return
#	their arguments, but it is always possible for them to modify the
#	value.)

#	These settor functions call primitives to set data structures used
#	inside of es.

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

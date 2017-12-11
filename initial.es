# initial.es -- set up initial interpreter state ($Revision: 1.1.1.1 $)

#
# Builtin functions
#

fn-.		    = $&dot
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

fn-%read	= $&read

fn-eval = @ {'{' ^ <={%flatten ' ' $*} ^ '}'}

fn-true		= result 0
fn-false	= result 1

fn-break	= throw break
fn-exit		= throw exit
fn-return	= throw return

fn-unwind-protect = $&noreturn @ body cleanup {
  %seq {
    if {%not {~ <={%count $cleanup} 1}} {
      throw error unwind-protect 'unwind-protect body cleanup'
    }
  } {
    let (exception)
      let (
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

fn-while = $&noreturn @ cond body {
	catch @ e value {
		if {%not {~ $e break}} {
			throw $e $value
		} {
		  result $value
    }
	} {
		let (result = <=true)
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
  let (res) %seq {
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
  let (res = $li(1)) %seq {
    apply @ i {res = $res^$sep^$i} $li(2 ...)
  } {
    result $res
  }
}

fn-%count	= $&count

fn-%seq = $&seq

fn-%not = $&noreturn @ cmd {
	if {$cmd} {false} {true}
}

fn-%and = $&noreturn @ first rest {
	let (result = <={$first})
		if {~ <={%count $rest} 0} {
			result $result
		} {result $result} {
			%and $rest
		} {
			result $result
		}
}

fn-%or = $&noreturn @ first rest {
	if {~ <={%count $first} 0} {
		false
	} {
		let (result = <={$first})
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

fn-%one = @ {
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

#	Input/Output substitution (i.e., the >{} and <{} forms) provide an
#	interesting case.  If es is compiled for use with /dev/fd, these
#	functions will be built in.  Otherwise, versions of the hooks are
#	provided here which use files in /tmp.
#
#	The /tmp versions of the functions are straightforward es code,
#	and should be easy to follow if you understand the rewriting that
#	goes on.  First, an example.  The pipe
#		ls | wc
#	can be simulated with the input/output substitutions
#		cp <{ls} >{wc}
#	which gets rewritten as (formatting added):
#		%readfrom _devfd0 {ls} {
#			%writeto _devfd1 {wc} {
#				cp $_devfd0 $_devfd1
#			}
#		}
#	What this means is, run the command {ls} with the output of that
#	command available to the {%writeto ....} command as a file named
#	by the variable _devfd0.  Similarly, the %writeto command means
#	that the input to the command {wc} is taken from the contents of
#	the file $_devfd1, which is assumed to be written by the command
#	{cp $_devfd0 $_devfd1}.
#
#	All that, for example, the /tmp version of %readfrom does is bind
#	the named variable (which is the first argument, var) to the name
#	of a (hopefully unique) file in /tmp.  Next, it runs its second
#	argument, input, with standard output redirected to the temporary
#	file, and then runs the final argument, cmd.  The unwind-protect
#	command is used to guarantee that the temporary file is removed
#	even if an error (exception) occurs.  (Note that the return value
#	of an unwind-protect call is the return value of its first argument.)
#
#	By the way, creative use of %newfd and %pipe would probably be
#	sufficient for writing the /dev/fd version of these functions,
#	eliminating the need for any builtins.  For now, this works.
#
#		cmd <{input}		%readfrom var {input} {cmd $var}
#		cmd >{output}		%writeto var {output} {cmd $var}

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

#	These versions of %readfrom and %writeto (contributed by Pete Ho)
#	support the use of System V FIFO files (aka, named pipes) on systems
#	that have them.  They seem to work pretty well.  The authors still
#	recommend using files in /tmp rather than named pipes.

#fn %readfrom var cmd body {
#	local ($var = /tmp/es.$var.$pid) {
#		unwind-protect {
#			/etc/mknod $$var p
#			$&background {$cmd > $$var; exit}
#			$body
#		} {
#			rm -f $$var
#		}
#	}
#}

#fn %writeto var cmd body {
#	local ($var = /tmp/es.$var.$pid) {
#		unwind-protect {
#			/etc/mknod $$var p
#			$&background {$cmd < $$var; exit}
#			$body
#		} {
#			rm -f $$var
#		}
#	}
#}


#
# Hook functions
#

#
# Read-eval-print loops
#

#	In es, the main read-eval-print loop (REPL) can lie outside the
#	shell itself.  Es can be run in one of two modes, interactive or
#	batch, and there is a hook function for each form.  It is the
#	responsibility of the REPL to call the parser for reading commands,
#	hand those commands to an appropriate dispatch function, and handle
#	any exceptions that may be raised.  The function %is-interactive
#	can be used to determine whether the most closely binding REPL is
#	interactive or batch.
#
#	The REPLs are invoked by the shell's main() routine or the . or
#	eval builtins.  If the -i flag is used or the shell determimes that
#	it's input is interactive, %interactive-loop is invoked; otherwise
#	%batch-loop is used.
#
#	The function %parse can be used to call the parser, which returns
#	an es command.  %parse takes two arguments, which are used as the
#	main and secondary prompts, respectively.  %parse typically returns
#	one line of input, but es allows commands (notably those with braces
#	or backslash continuations) to continue across multiple lines; in
#	that case, the complete command and not just one physical line is
#	returned.
#
#	By convention, the REPL must pass commands to the fn %dispatch,
#	which has the actual responsibility for executing the command.
#	Whatever routine invokes the REPL (internal, for now) has
#	the resposibility of setting up fn %dispatch appropriately;
#	it is used for implementing the -e, -n, and -x options.
#	Typically, fn %dispatch is locally bound.
#
#	The %parse function raises the eof exception when it encounters
#	an end-of-file on input.  You can probably simulate the C shell's
#	ignoreeof by restarting appropriately in this circumstance.
#	Other than eof, %interactive-loop does not exit on exceptions,
#	where %batch-loop does.
#
#	The looping construct forever is used rather than while, because
#	while catches the break exception, which would make it difficult
#	to print ``break outside of loop'' errors.
#
#	The parsed code is executed only if it is non-empty, because otherwise
#	result gets set to zero when it should not be.

fn-%parse	= $&parse
fn-%batch-loop	= $&batchloop
fn-%is-interactive = $&isinteractive

fn-%interactive-loop = @ {
	let (result = <=true)
		catch @ e type msg {
      %seq {
        if {~ $e eof} {
          return $result
        } {~ $e exit} {
          throw $e $type $msg
        } {~ $e error} {
          %seq {
            %dup 1 2 {echo $msg}
          } {
            $fn-%dispatch false
          }
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
        %seq {
          if {%not {~ <={%count $fn-%prompt} 0}} {
            %prompt
          }
        } {
          let (code = <={%parse $prompt})
            if {%not {~ <={%count $code} 0}} {
              result = <={$fn-%dispatch $code}
            }
        }
			}
		}
}

#	These functions are potentially passed to a REPL as the %dispatch
#	function.  (For %eval-noprint, note that an empty list prepended
#	to a command just causes the command to be executed.)

fn-%eval-noprint    =
fn-%eval-print	    = @ {%seq {%dup 1 2 {echo $*}} {$*}}
fn-%noeval-noprint	= @ {}			# -n
fn-%noeval-print    = @ {%dup 1 2 {echo $*}}	# -n -x
fn-%exit-on-false   = $&exitonfalse		# -e


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
prompt		= '; ' ''
max-eval-depth	= 640

#
# Title
#

#	This is silly and useless, but whatever value is returned here
#	is printed in the header comment in initial.c;  nobody really
#	wants to look at initial.c anyway.

result es-core initial state built for <=$&version

#
# Read-eval-print loops
#

# In es, the main read-eval-print loop (REPL) can lie outside the
# shell itself.  Es can be run in one of two modes, interactive or
# batch, and there is a hook function for each form.  It is the
# responsibility of the REPL to call the parser for reading commands,
# hand those commands to an appropriate dispatch function, and handle
# any exceptions that may be raised.  The function %is-interactive
# can be used to determine whether the most closely binding REPL is
# interactive or batch.
#
# The REPLs are invoked by the shell's main() routine or the . or
# eval builtins.  If the -i flag is used or the shell determines that
# it's input is interactive, %interactive-loop is invoked; otherwise
# %batch-loop is used.
#
# The function %parse can be used to call the parser, which returns
# an es command.  %parse takes two arguments, which are used as the
# main and secondary prompts, respectively.  %parse typically returns
# one line of input, but es allows commands (notably those with braces
# or backslash continuations) to continue across multiple lines; in
# that case, the complete command and not just one physical line is
# returned.
#
# By convention, the REPL must pass commands to the fn %dispatch,
# which has the actual responsibility for executing the command.
# Whatever routine invokes the REPL (internal, for now) has
# the resposibility of setting up fn %dispatch appropriately;
# it is used for implementing the -e, -n, and -x options.
# Typically, fn %dispatch is locally bound.
#
# The %parse function raises the eof exception when it encounters
# an end-of-file on input.  You can probably simulate the C shell's
# ignoreeof by restarting appropriately in this circumstance.
# Other than eof, %interactive-loop does not exit on exceptions,
# where %batch-loop does.
#
# The looping construct forever is used rather than while, because
# while catches the break exception, which would make it difficult
# to print ``break outside of loop'' errors.
#
# The parsed code is executed only if it is non-empty, because otherwise
# result gets set to zero when it should not be.

fn-%parse = $&parse
fn-%is-interactive = $&isinteractive

fn %batch-loop {
  let (result = <=true) {
    catch @ e rest {
      if {~ $e eof} {
        return $result
      } {
        throw $e $rest
      }
    } {
      forever {
        let (cmd = <=%parse) {
          if {!~ $#cmd 0} {
            result = <={$fn-%dispatch $cmd}
          }
        }
      }
    }
  }
}

fn %interactive-loop {
  let (result = <=true) {
    catch @ e type msg {
      if {~ $e eof} {
        return $result
      } {~ $e exit} {
        throw $e $type $msg
      } {~ $e error} {
        echo >[1=2] $msg
        $fn-%dispatch false
      } {~ $e signal} {
        if {!~ $type sigint sigterm sigquit} {
          echo >[1=2] caught unexpected signal: $type
        }
      } {
        echo >[1=2] uncaught exception: $e $type $msg
      }
      throw retry # restart forever loop
    } {
      forever {
        if {!~ $#fn-%prompt 0} {
          catch @ e rest {
            if {!~ $e false} {
              echo >[1=2] 'caught exception in %prompt:' $e $rest
            }
          } {%prompt}
        }
        let (code = <={%parse $prompt}) {
          if {!~ $#code 0} {
            status = <={$fn-%interactive-dispatch $fn-%dispatch $code}
          }
        }
      }
    }
  }
}

# These functions are potentially passed to a REPL as the %dispatch
# function.  (For %eval-noprint, note that an empty list prepended
# to a command just causes the command to be executed.)

fn %eval-noprint                             # <default>
fn %eval-print       { echo $* >[1=2]; $* }  # -x
fn %noeval-noprint   { }                     # -n
fn %noeval-print     { echo $* >[1=2] }      # -n -x
fn-%throw-on-false = $&throwonfalse          # -e


# Likely migration path:
#
# State 0 (Current state):
#
# - main() calls runfd() or runstring(); those set the correct input, then they call:
# - runinput(), which sets the dispatcher up, and calls either fn-%interactive-loop,
#   fn-%batch-loop, or $&batchloop
# - $&dot does some argument/runflags stuff, opens a file, then calls runfd()
# - $&exec just calls eval() internally
# - fn-eval just causes some glom/eval stuff to happen internally
#

# State 1:
#
# - runflags are exposed to es as fn args
# - main() calls runfd() or runstring(), which set the corect input, and call
# - fn-%run-input, which sets the dispatcher up, and calls either fn-%interactive-loop or
#   fn-%batch-loop
# - $&dot calls runfd(), which calls %run-input transitively
# - ./esdump uses a simplified $&batchloop to operate (TODO: give ./esdump private
#   bootstrapping prims?)


# Run an input file.  Initially, this fn does not actually set the input file itself,
# but does take runflags as args.  In the future, this will have signature
# `fn %run-input file`, and if ~ $#file 0, it will run stdin; runflags will be a special global.
fn %run-input runflags {
  # Here we manually use $fn-<whatever>-loop rather than <whatever>-loop, to
  # prevent messing with $0
  let (
    dispatch-p = <={if {~ $runflags printcmds} {result 'print'} {result 'noprint'}}
    dispatch-e = <={if {~ $runflags noexec}    {result 'noeval'} {result 'eval'}}
    on-false   = <={if {~ $runflags throwonfalse} {result $fn-%throw-on-false} {result ()}}
    loop       = <={if {~ $runflags interactive} {result $fn-%interactive-loop} {result $fn-%batch-loop}}
  ) {
    local (fn-%dispatch = $on-false $(fn-%^$(dispatch-e)^-^$(dispatch-p))) {
      $loop
    }
  }
}

noexport = $noexport fn-%dispatch


# State 2:
#
# - ~all runflags are exposed to es as a global, with appropriate settor logic
# - standard es dispatch changes behavior live based on runflags
# - main() calls es:main, which calls fn-%run-input, which calls the $&setinput primitive &c.
# - $&dot is replaced with an es wrapper for fn-%run-input
#

# State 3 (sort of a state):
#
# - main() is migrated, bottom to top, into es:main
# - eventually including arg parsing
#

# runflags:
#
#   inchild      - currently in child (e.g., don't fork on fork/exec)
#   throwonfalse - exit on false                 -e
#   interactive  - forced to interactive         -i
#   noexec       - don't execute commands        -n
#   echoinput    - echoinput to stderr           -v
#   printcmds    - print commands                -x
#   lisptrees    - print commands as lisp trees  -L
#
#   inchild      - internal (rw), NOT exported, very evil
#   throwonfalse - internal (rw), so things like $&if can disable/reenable for each block
#   interactive  - internal (ro), to control things like readline enabling
#   noexec       - not internal
#   echoinput    - internal (ro), requires input support
#   printcmds    - not internal
#   lisptrees    - internal (ro), requires input support
#
# other flags:
#   -l  - login shell (also if $0[0] == '-')
#   -p  - protected shell (do not load fns from env)
#   -d  - do not catch SIGINT or SIGQUIT
#   -s  - read cmds from stdin, stop parsing args
#   -o  - keep stdout, stdin, and stderr closed if not already open
#   -c  - cmd to run, babey

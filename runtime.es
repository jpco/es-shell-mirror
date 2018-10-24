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
          catch @ _ _ e {
            echo 'caught error in %prompt: '^$e
          } {%prompt}
        }
        let (code = <={%parse $prompt}) {
          if {!~ $#code 0} {
            status = <={$fn-%dispatch $code}
          }
        }
      }
    }
  }
}

# These functions are potentially passed to a REPL as the %dispatch
# function.  (For %eval-noprint, note that an empty list prepended
# to a command just causes the command to be executed.)

fn %eval-noprint                            # <default>
fn %eval-print      { echo $* >[1=2]; $* }  # -x
fn %noeval-noprint  { }                     # -n
fn %noeval-print    { echo $* >[1=2] }      # -n -x
fn-%exit-on-false = $&exitonfalse           # -e


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
  let (
    dispatch-p = 'noprint'
    dispatch-e = 'eval'
    on-false   = ()
    loop       = %batch-loop
  ) {
    if {~ $runflags printcmds}   {dispatch-p = print}
    if {~ $runflags noexec}      {dispatch-e = noeval}
    if {~ $runflags exitonfalse} {on-false = %exit-on-false}
    if {~ $runflags interactive} {loop = %interactive-loop}
    if {~ $#(fn-^$loop) 0}       {loop = $&batchloop}

    local (fn-%dispatch = $(fn-%^$(dispatch-e)^-^$(dispatch-p))) {
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

#
# runflags:
#
#   inchild     - currently in a child context
#
# controls some redirection behavior, forking behavior, etc.
# kind of all over, probably not to be messed with.
#
#   exitonfalse - exit on false                 -e
#
# exits if eval call turns out false.
# TODO: replace with a handleable 'throw on false'?
#
#   interactive - forced to interactive         -i
#
# if true, use readline, use fn-%interactive-loop, change error printing
#
#   noexec      - don't execute commands        -n
#
# fn-%dispatch is set to a noexec version if set
#
#   echoinput   - echoinput to stderr           -v
#
# print input as read (input.c:203)
#
#   printcmds   - print commands                -x
#
# fn-%dispatch is set to an echo version if set
#
#   lisptrees   - print commands as lisp trees  -L
#
# print parsed input as lisp tree after parsing (input.c:402)
#
# other flags:
#   -l  - login shell (also if $0[0] == '-')
#   -p  - protected shell (do not load fns from env)
#   -d  - do not catch SIGINT or SIGQUIT
#   -s  - read cmds from stdin, stop parsing args
#   -o  - keep stdout, stdin, and stderr closed if not already open
#   -c  - cmd to run, babey


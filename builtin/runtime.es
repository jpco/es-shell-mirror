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

fn %is-interactive {
  ~ $runflags interactive
}

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

fn %eval-noprint                             # <default>
fn %eval-print       { echo $* >[1=2]; $* }  # -x
fn %noeval-noprint   { }                     # -n
fn %noeval-print     { echo $* >[1=2] }      # -n -x
fn-%throw-on-false = $&throwonfalse          # -e

#
# Es entry points
#

# Es execution is configured with the $runflags variable.  $runflags
# can contain arbitrary terms, but only 'throwonfalse', 'interactive',
# 'noexec', 'echoinput', 'printcmds', and 'lisptrees' (if support is
# compiled in) do anything by default.

set-runflags = @ new {
  let (nf = ()) {
    for (flag = $new) {
      if {!~ $nf $flag} {nf = $nf $flag}
    }
    let (
      dp-p = <={if {~ $nf printcmds} {result 'print'} {result 'noprint'}}
      dp-e = <={if {~ $nf noexec} {result 'noeval'} {result 'eval'}}
      dp-f = <={if {~ $nf throwonfalse} {result '%throw-on-false'} {result ()}}
    ) fn-%dispatch = $dp-f $(fn-%^$(dp-e)^-^$(dp-p))
    $&setrunflags $nf
  }
}

noexport = $noexport fn-%dispatch runflags

fn %dot args {
  let (
    fn usage {
      throw error %dot 'usage: . [-einvx] file [arg ...]'
    }
    flags = ()
  ) {
    for (a = $args) {
      if {!~ $a -*} {
        break
      }
      args = $args(2 ...)
      if {~ $a --} {
        break
      }
      for (f = <={%fsplit '' <={~~ $a -*}}) {
        if (
          {~ $f e} {flags = $flags throwonfalse}
          {~ $f i} {flags = $flags interactive}
          {~ $f n} {flags = $flags noexec}
          {~ $f v} {flags = $flags echoinput}
          {~ $f x} {flags = $flags printcmds}
            {usage}
        )
      }
    }
    if {~ $#args 0} {usage}
    local (
      0 = $args(1)
      * = $args(2 ...)
      runflags = $flags
    ) $fn-%run-input $args(1)
  }
}

fn-. = %dot

fn %run-input file {
  $&runinput {
    if %is-interactive {
      $fn-%interactive-loop
    } {
      $fn-%batch-loop
    }
  } $file
}

# es:main takes bools 'cmd' and 'stdin' and args.
# if ~ $stdin true, then $args becomes $* and stdin is run.
# if ~ $cmd true, then $args(1) is the command to be run.
es:main = @ cmd stdin args {
  if {!$cmd && !$stdin && !~ $#args 0} {
    local ((0 *) = $args)
      $fn-%run-input $0
  } {$cmd} {
    local (* = $args(2 ...))
      $fn-eval $args(1)
  } {
    local (* = $args)
      $fn-%run-input
  }
}

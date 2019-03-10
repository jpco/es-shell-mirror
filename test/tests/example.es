#!/usr/local/bin/es --

test = @ {
  let (res = ()) for (a = $*) {
    catch {res = $res $a} {
      res = $res `($a + 1)
    }
  }
}

case 1 fish
want 2 fish

case -3 fish making 12.5 cookies
want -2 fish making 13.5 cookies

## Old Project

This project was found at an archeological digsite covering a little-explored corner of my hard drive.  It is well past its expiration date and in particular may no longer represent anyone's opinion 
about best-practices for either coding or functionality.  It is here in case the idea or code may have some value to someone, but the activation energy required for restarting actual development on this 
project is currently quite high.

## README

```
20110606_potato_0.7: 

COMPILE: 
gcc -lX11 -lpthread potato.c -o potato

RUN: 
Standalone: ./potato
As server:  ./potato [listening port]
As client:  ./potato [server IP] [server port]

KEYBOARD COMMANDS: 
qqq            --  quit.  
z,x,c,v,b,n,m  --  Set line width
0-9            --  Set colour
l              --  Clear screen

CHANGES: 
- Uses pixmap for double-buffering/refreshing
- Exits cleanly when 'X' button used to close
- Sets window title to show thickness, colour
- Properly unbinds port for reuse after close

ISSUES: 
- Unknown compatability with multiple monitors
- This was thrown together at high speed and so may be highly insecure--be careful!
```

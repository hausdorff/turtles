clean:
	-@rm turtles.o65 2> /dev/null
#	-@rm a2lisp

# Expand macros with CPP, then remove all newlines and CPP junk
compile:
#	@gcc -Wall -std=gnu99 a2lisp.c -o a2lisp
	@cl65 -tapple2 -C apple2-system.cfg a2lisp.c -o a2lisp
#	@xa turtles.asm -M -o turtles.o65

send: compile
	@./transport a2lisp | sox -b 8 -r 44100 -L -c 1 -t raw -e unsigned-integer - -d

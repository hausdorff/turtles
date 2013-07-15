clean:
	-@rm turtles_expanded.asm 2> /dev/null
	-@rm turtles.o65 2> /dev/null

# Expand macros with CPP, then remove all newlines and CPP junk
compile:
	@cpp turtles.asm | sed -E 's/(;.*)|(^#.*)//' | awk /./{print} > turtles_expanded.asm
	@xa turtles_expanded.asm -o turtles.o65

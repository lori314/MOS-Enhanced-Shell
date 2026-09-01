INITAPPS += 

# Shared path-resolution and shell I/O libraries.
USERLIB	+= lib/path.o \
           lib/shellio.o

USERAPPS += touch.b \
           mkdir.b \
           rm.b
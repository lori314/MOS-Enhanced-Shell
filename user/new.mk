INITAPPS += 

# Add ONLY the object file for our new, shared path resolution library.
# We will add other shell modules much later.
USERLIB	+= lib/path.o \
           lib/shellio.o

USERAPPS += touch.b \
           mkdir.b \
           rm.b
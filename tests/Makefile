DIRS=	struct libkern vfs mm

target:	test

test:	framework/framework.a
	for i in $(DIRS); do \
		echo * Testing $$i; \
		cd $$i; $(MAKE) test; cd ..; \
	done

clean:
	(cd framework; $(MAKE) clean; cd ..)
	@for i in $(DIRS); do \
		cd $$i; $(MAKE) clean; cd ..; \
	done

framework/framework.a:	framework/Makefile
	(cd framework; $(MAKE); cd ..)

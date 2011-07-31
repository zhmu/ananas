DIRS=	struct libkern vfs mm

target:	test

build:	framework/framework.a
	for i in $(DIRS); do \
		echo \* Building $$i; \
		cd $$i; $(MAKE); cd ..; \
	done

test:	build
	for i in $(DIRS); do \
		echo \* Begin test: $$i; \
		cd $$i; $(MAKE) test; cd ..; \
		echo \* Done test: $$i; \
	done

clean:
	(cd framework; $(MAKE) clean; cd ..)
	@for i in $(DIRS); do \
		cd $$i; $(MAKE) clean; cd ..; \
	done

framework/framework.a:	framework/Makefile
	(cd framework; $(MAKE); cd ..)

MODULES = \
	cf2trace \
	dif2trace \
	peak2trig \
	postshake \
	respectra \
	shakemap \
	shake2redis \
	shake2ws \
	trace2peak

#####################
# Top level targets #
#####################

# There is no default target; print usage info
usage: PHONY
	@echo "Usage: make unix or unix_v710"

unix: unix_libs unix_modules

unix_v710: unix_libs unix_modules_v710

###############################
# Unix/Linux/Mac OS X targets #
###############################

unix_libs: MAKING_C_LIBRARIES_MESSAGE
	@cd src/libsrc && \
		$(MAKE) -f makefile.unix

unix_modules: MAKING_MODULES_MESSAGE
	@for x in $(MODULES) ; \
	do \
		cd src/$$x && \
			echo ---------- ; \
			echo Making $@ in: `pwd` ; \
			$(MAKE) -f makefile.unix || exit "$$?" ; \
			cd - ; \
	done

unix_modules_v710: MAKING_MODULES_MESSAGE
	@for x in $(MODULES) ; \
	do \
		cd src/$$x && \
			echo ---------- ; \
			echo Making $@ in: `pwd` ; \
			$(MAKE) -f makefile.unix ver_710 || exit "$$?" ; \
			cd - ; \
	done

clean_unix: PHONY
	-@cd src/libsrc && \
		echo Cleaning in: `pwd` ; \
		$(MAKE) -f makefile.unix clean
	-@for x in $(MODULES) ; \
	do ( \
		cd src/$$x && \
			echo Cleaning in: `pwd` ; \
			$(MAKE) -f makefile.unix clean \
	) ; done

clean_bin_unix: PHONY
	-@cd src/libsrc && \
		echo Cleaning libraries in: `pwd` ; \
		$(MAKE) -f makefile.unix clean_lib
	-@for x in $(MODULES) ; \
	do ( \
		cd src/$$x && \
			echo Cleaning binaries in: `pwd` ; \
			$(MAKE) -f makefile.unix clean_bin \
	) ; done

##################
# Helper targets #
##################

MAKING_C_LIBRARIES_MESSAGE: PHONY
	@echo -*-*-*-*-*-*-*-*-*
	@echo Making C libraries
	@echo -*-*-*-*-*-*-*-*-*

MAKING_MODULES_MESSAGE: PHONY
	@echo -*-*-*-*-*-*-*-*-*-*-*
	@echo Making P-Alert modules
	@echo -*-*-*-*-*-*-*-*-*-*-*

PHONY:

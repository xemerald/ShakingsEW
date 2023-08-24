MODULES = \
	cf2trace \
	dif2trace \
	peak2trig \
	respectra \
	shake2redis \
	shake2ws \
	trace2peak
#	postshake
#	shakemap

MODULES_DB = \
	cf2trace \
	peak2trig

#####################
# Top level targets #
#####################

# There is no default target; print usage info
usage: PHONY
	@echo "Usage: make unix or unix_sql"

unix: unix_libs unix_modules

unix_sql: unix_libs_sql unix_modules_sql

###############################
# Unix/Linux/Mac OS X targets #
###############################

unix_libs: MAKING_C_LIBRARIES_MESSAGE
	@cd src/libsrc && \
		$(MAKE) -f makefile.unix

unix_libs_sql: MAKING_DB_LIBRARIES_MESSAGE
	@cd src/libsrc && \
		$(MAKE) -f makefile.unix libdblist

unix_modules: MAKING_MODULES_MESSAGE
	@for x in $(MODULES) ; \
	do \
		cd src/$$x && \
			echo ---------- ; \
			echo Making $@ in: `pwd` ; \
			$(MAKE) -f makefile.unix ver_710 || exit "$$?" ; \
			cd - ; \
	done

unix_modules_sql: MAKING_MODULES_MESSAGE
	@for x in $(MODULES_DB) ; \
	do \
		cd src/$$x && \
			echo ---------- ; \
			echo Making $@ in: `pwd` ; \
			$(MAKE) -f makefile.unix clean || exit "$$?" ; \
			$(MAKE) -f makefile.unix ver_710_sql || exit "$$?" ; \
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

MAKING_DB_LIBRARIES_MESSAGE: PHONY
	@echo -*-*-*-*-*-*-*-*-*-*-*-*-*
	@echo Making Database libraries
	@echo -*-*-*-*-*-*-*-*-*-*-*-*-*

MAKING_MODULES_MESSAGE: PHONY
	@echo -*-*-*-*-*-*-*-*-*-*-*-*-*-*-*
	@echo Making Shaking System modules
	@echo -*-*-*-*-*-*-*-*-*-*-*-*-*-*-*

PHONY:

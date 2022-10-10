default : all

.PHONY: default all clean install test

include $(TOPDIR)/src/include/version.mk

comma:= ,
empty:=
space:= $(empty) $(empty)

set-if-undefined = $(call eval,$(1) := $(if $(filter default undefined,$(origin $(1))),$(2),$($(1))))
add-prefix = $(subst $(space),$(empty),$(1)$(foreach x,$(2),$(comma)$(x)))

FAMILY_SUFFIXES = $(if $(findstring clang,$(CC)),CLANG,) \
		  $(if $(findstring ccc-analyzer,$(CC)),CCC_ANALYZER,) \
		  $(if $(findstring gcc,$(CC)),GCC,)
family = $(foreach FAMILY_SUFFIX,$(FAMILY_SUFFIXES),$($(1)_$(FAMILY_SUFFIX)))

%.a :
	$(AR) -cvqs $@ $^

%.1 : %.1.mdoc
	$(MANDOC) -mdoc -Tman -Ios=Linux $^ > $@

%.3 : %.3.mdoc
	$(MANDOC) -mdoc -Tman -Ios=Linux $^ > $@

% : %.c

% : %.o
	$(CCLD) $(CCLDFLAGS) $(CPPFLAGS) -o $@ $(sort $^) $(LDLIBS)

%-static : CCLDFLAGS+=-static
%-static : %.o
	$(CCLD) $(CCLDFLAGS) $(CPPFLAGS) -o $@ $(sort $^) $(LDLIBS)

%.so :
	$(CCLD) $(CCLDFLAGS) $(CPPFLAGS) $(SOFLAGS) -o $@ $^ $(LDLIBS)
	ln -vfs $@ $@.1

%.abixml : %.so
	$(ABIDW) --headers-dir $(TOPDIR)/src/include/efivar/ --out-file $@ $^
	@sed -i -s 's,$(TOPDIR)/,,g' $@

%.abicheck : %.so
	$(ABIDIFF) \
		--suppr abignore \
		--headers-dir2 $(TOPDIR)/src/include/efivar/ \
		$(patsubst %.so,%.abixml,$<) \
		$<

%.o : %.c
	$(CC) $(CFLAGS) -fPIC $(CPPFLAGS) -c -o $@ $(filter %.c %.o %.S,$^)

%.static.o : %.c
	$(CC) $(CFLAGS) -fPIE $(CPPFLAGS) -c -o $@ $(filter %.c %.o %.S,$^)

%.o : %.S
	$(CC) $(CFLAGS) -fPIC $(CPPFLAGS) -c -o $@ $(filter %.c %.o %.S,$^)

%.static.o : %.S
	$(CC) $(CFLAGS) -fPIE $(CPPFLAGS) -c -o $@ $(filter %.c %.o %.S,$^)

%.S: %.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -S $< -o $@

%.E: %.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -E $< -o $@

%.c : %.h

define substitute-version
	sed						\
		-e "s,@@VERSION@@,$(VERSION),g"		\
		-e "s,@@LIBDIR@@,$(LIBDIR),g"		\
		-e "s,@@PREFIX@@,$(PREFIX),g"		\
		-e "s,@@EXEC_PREFIX@@,$(EXEC_PREFIX),g"		\
		-e "s,@@INCLUDEDIR@@,$(INCLUDEDIR),g"		\
		$(1) > $(2)
endef

%.pc : %.pc.in
	@$(call substitute-version,$<,$@)
%.spec : %.spec.in
	@$(call substitute-version,$<,$@)
%.map : %.map.in
	@$(call substitute-version,$<,$@)

pkg-config-cflags = $(if $(PKGS),$(shell $(PKG_CONFIG) --cflags $(PKGS)))
pkg-config-ccldflags = $(if $(PKGS),$(shell $(PKG_CONFIG) --libs-only-L --libs-only-other $(PKGS)))
pkg-config-ldlibs = $(if $(PKGS),$(shell $(PKG_CONFIG) --libs-only-l $(PKGS)))

deps-of = $(foreach src,$(filter %.c,$(1)),$(patsubst %.c,.%.d,$(src))) \
	  $(foreach src,$(filter %.S,$(1)),$(patsubst %.S,.%.d,$(src)))

get-config = $(shell git config --local --get "efivar.$(1)")

# vim:ft=make

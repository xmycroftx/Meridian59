#
# overall makefile
#

TOPDIR=.
!include common.mak

.SILENT:

# make ignores targets if they match directory names
all: APrep Bzlib Blibpng Bjansson Bserver Bclient Bmodules Bkod Bdeco Bbbgun Bkeybind Bresource
APrep: 
    setlocal enableextensions
    IF exist $(CLIENTRUNDIR)\resource ( echo resources dir exists ) ELSE ( mkdir $(CLIENTRUNDIR)\resource && echo resources dir created)
	IF exist $(BLAKSERVRUNDIR)\rsc ( echo server\rsc dir exists ) ELSE ( mkdir $(BLAKSERVRUNDIR)\rsc && echo server\rsc dir created)
	IF exist $(BLAKSERVRUNDIR)\memmap ( echo server\memmap dir exists ) ELSE ( mkdir $(BLAKSERVRUNDIR)\memmap && echo server\memmap dir created > $(BLAKSERVRUNDIR)\memmap\.placeholder )
	IF exist $(BLAKSERVRUNDIR)\loadkod ( echo server\loadkod dir exists ) ELSE ( mkdir $(BLAKSERVRUNDIR)\loadkod && echo server\loadkod dir created )
	IF exist $(BLAKSERVRUNDIR)\savegame ( echo server\savegame dir exists ) ELSE ( mkdir $(BLAKSERVRUNDIR)\savegame && echo server\savegame dir created > $(BLAKSERVRUNDIR)\savegame\.placeholder )
	IF exist $(BLAKSERVRUNDIR)\channel ( echo server\channel dir exists ) ELSE ( mkdir $(BLAKSERVRUNDIR)\channel && echo server\channel dir created > $(BLAKSERVRUNDIR)\channel\.placeholder )

Bserver: Bresource Bjansson
	echo Making $(COMMAND) in $(BLAKSERVDIR)
	cd $(BLAKSERVDIR)
	$(MAKE) /$(MAKEFLAGS) $(COMMAND)
	cd ..

Bclient: Butil Bresource
	echo Making $(COMMAND) in $(CLIENTDIR)
	cd $(CLIENTDIR)
	$(MAKE) /$(MAKEFLAGS) $(COMMAND)
	cd ..
	$(CP) $(BLAKBINDIR)\club.exe $(CLIENTRUNDIR)
!if !DEFINED(NOCOPYFILES)
# Postbuild handles its own echoes
	$(POSTBUILD)
!endif NOCOPYFILES

Bmodules: Bclient
	echo Making $(COMMAND) in $(MODULEDIR)
	cd $(MODULEDIR)
	$(MAKE) /$(MAKEFLAGS) $(COMMAND)
	cd ..

Bcompiler:
	echo Making $(COMMAND) in $(BLAKCOMPDIR)
	cd $(BLAKCOMPDIR)
	$(MAKE) /$(MAKEFLAGS) $(COMMAND)
	cd ..

Bdiff:
	echo Making $(COMMAND) in $(DIFFDIR)
	cd $(DIFFDIR)
	$(MAKE) /$(MAKEFLAGS) $(COMMAND)
	cd ..

Bkod: Bdiff Bcompiler
	echo Making $(COMMAND) in $(KODDIR)
	cd $(KODDIR)
	$(MAKE) /$(MAKEFLAGS) $(COMMAND)
	cd ..

Bdeco:
	echo Making $(COMMAND) in $(DECODIR)
	cd $(DECODIR)
	$(MAKE) /$(MAKEFLAGS) $(COMMAND)
	cd ..

Bresource: Bmakebgf Bbbgun
	echo Making $(COMMAND) in $(RESOURCEDIR)
	cd $(RESOURCEDIR)
	$(MAKE) /$(MAKEFLAGS) $(COMMAND)
	cd ..

Bmakebgf:
	echo Making $(COMMAND) in $(MAKEBGFDIR)
	cd $(MAKEBGFDIR)
	$(MAKE) /$(MAKEFLAGS) $(COMMAND)
	cd ..

Butil: Bjansson
	echo Making $(COMMAND) in $(UTILDIR)
	cd $(UTILDIR)
	$(MAKE) /$(MAKEFLAGS) $(COMMAND)
	cd ..

Bbbgun:
	echo Making $(COMMAND) in $(BBGUNDIR)
	cd $(BBGUNDIR)
	$(MAKE) /$(MAKEFLAGS) $(COMMAND)
	cd ..

Bkeybind:
	echo Making $(COMMAND) in $(KEYBINDDIR)
	cd $(KEYBINDDIR)
	$(MAKE) /$(MAKEFLAGS) $(COMMAND)
	cd ..
	
Blibpng:
	echo Making $(COMMAND) in $(LIBPNGDIR)
	cd $(LIBPNGDIR)
	$(MAKE) /$(MAKEFLAGS) $(COMMAND)
	cd ..
	
Bzlib:
	echo Making $(COMMAND) in $(ZLIBDIR)
	cd $(ZLIBDIR)
	$(MAKE) /$(MAKEFLAGS) $(COMMAND)
	cd ..

Bjansson:
	echo Making $(COMMAND) in $(JANSSONDIR)
	cd $(JANSSONDIR)
	$(MAKE) /$(MAKEFLAGS) $(COMMAND)
	cd ..

clean:
        set NOCOPYFILES=1
        set COMMAND=clean
        $(MAKE) /$(MAKEFLAGS)
		$(RM) $(TOPDIR)\postbuild.log >nul 2>&1
		$(RM) $(BLAKSERVDIR)\channel\*.txt 2>nul
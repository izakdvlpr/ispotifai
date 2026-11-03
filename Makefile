#---------------------------------------------------------------------------------
# iSpotifai - builds the .nro client, the .ovl Tesla overlay and the sysmodule
#
#   make            build all three, stage into dist/
#   make app        build only app/ispotifai.nro
#   make overlay    build only overlay/ispotifai.ovl
#   make sysmodule  build only sysmodule/ispotifai.nsp
#   make run        send the .nro to the Switch and run it (needs SWITCH_IP)
#   make install    upload dist/ to the Switch over FTP (needs SWITCH_IP)
#   make clean      wipe build output
#---------------------------------------------------------------------------------
SWITCH_IP   ?=
SWITCH_PORT ?= 5000

DIST := dist

# 0x42 is the homebrew title id range; 49 53 50 is "ISP" in ASCII
TITLE_ID := 4200000000495350

.PHONY: all app overlay sysmodule dist run install clean

all: dist

app:
	@$(MAKE) --no-print-directory -C app

overlay:
	@$(MAKE) --no-print-directory -C overlay

sysmodule:
	@$(MAKE) --no-print-directory -C sysmodule

dist: app overlay sysmodule
	@mkdir -p $(DIST)/switch/ispotifai $(DIST)/switch/.overlays
	@mkdir -p $(DIST)/atmosphere/contents/$(TITLE_ID)/flags
	@cp app/ispotifai.nro       $(DIST)/switch/ispotifai/ispotifai.nro
	@cp overlay/ispotifai.ovl   $(DIST)/switch/.overlays/ispotifai.ovl
	@cp sysmodule/ispotifai.nsp $(DIST)/atmosphere/contents/$(TITLE_ID)/exefs.nsp
	@cp sysmodule/toolbox.json  $(DIST)/atmosphere/contents/$(TITLE_ID)/toolbox.json
	@touch $(DIST)/atmosphere/contents/$(TITLE_ID)/flags/boot2.flag
	@echo "staged ... $(DIST)/"

run: app
	@[ -n "$(SWITCH_IP)" ] || (echo "set SWITCH_IP=<console ip>"; exit 1)
	nxlink -s -a $(SWITCH_IP) app/ispotifai.nro

install: dist
	@[ -n "$(SWITCH_IP)" ] || (echo "set SWITCH_IP=<console ip>"; exit 1)
	curl -s --ftp-create-dirs -T $(DIST)/switch/ispotifai/ispotifai.nro \
		ftp://$(SWITCH_IP):$(SWITCH_PORT)/switch/ispotifai/ispotifai.nro
	curl -s --ftp-create-dirs -T $(DIST)/switch/.overlays/ispotifai.ovl \
		ftp://$(SWITCH_IP):$(SWITCH_PORT)/switch/.overlays/ispotifai.ovl
	curl -s --ftp-create-dirs -T $(DIST)/atmosphere/contents/$(TITLE_ID)/exefs.nsp \
		ftp://$(SWITCH_IP):$(SWITCH_PORT)/atmosphere/contents/$(TITLE_ID)/exefs.nsp
	curl -s --ftp-create-dirs -T $(DIST)/atmosphere/contents/$(TITLE_ID)/toolbox.json \
		ftp://$(SWITCH_IP):$(SWITCH_PORT)/atmosphere/contents/$(TITLE_ID)/toolbox.json
	curl -s --ftp-create-dirs -T $(DIST)/atmosphere/contents/$(TITLE_ID)/flags/boot2.flag \
		ftp://$(SWITCH_IP):$(SWITCH_PORT)/atmosphere/contents/$(TITLE_ID)/flags/boot2.flag
	@echo "uploaded to $(SWITCH_IP) -- reboot the console to start the sysmodule"

clean:
	@$(MAKE) --no-print-directory -C app clean
	@$(MAKE) --no-print-directory -C overlay clean
	@$(MAKE) --no-print-directory -C sysmodule clean
	@rm -rf $(DIST)

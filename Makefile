PS5_PAYLOAD_SDK ?= $(CURDIR)/.deps/ps5-payload-sdk
LLVM_CONFIG ?= llvm-config
export LLVM_CONFIG
.PHONY: all host test clean
all: releases/ps5-trophies-v0.2.3.elf
payload/assets.h: web/index.html web/app.js web/model.js web/style.css tools/embed.py
	python3 tools/embed.py
releases/ps5-trophies-v0.2.3.elf: payload/main.c payload/storage.c payload/profile.c payload/lifecycle.c payload/lifecycle.h payload/profile.h payload/storage.h payload/assets.h
	$(PS5_PAYLOAD_SDK)/bin/prospero-clang -std=gnu11 -O2 -Wall -Wextra -Werror -o $@ payload/main.c payload/storage.c payload/profile.c payload/lifecycle.c -lpthread -lSceUserService
host: build/trophy-server
build/trophy-server: payload/main.c payload/storage.c payload/profile.c payload/lifecycle.c payload/lifecycle.h payload/profile.h payload/storage.h payload/assets.h
	mkdir -p build
	cc -std=gnu11 -DHOST_BUILD -O2 -Wall -Wextra -Werror -o $@ payload/main.c payload/storage.c payload/profile.c payload/lifecycle.c -lpthread
test: host
	node --test tests/model.test.mjs
	python3 -m unittest discover -s tests -p 'test_*.py' -v
clean:
	rm -f build/trophy-server payload/assets.h releases/ps5-trophies-v0.2.3.elf

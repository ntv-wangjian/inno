all:
	g++ -Wall sloader.c miscs.c -o  ntv_sloader
	g++ -Wall ntv_prun.c miscs.c -o ntv_prun

install:
	cp ./ntv_sloader /usr/local/bin/ntv_sloader
	cp ./ntv_prun /usr/local/bin/ntv_prun

uninstall:
	rm /usr/local/bin/ntv_sloader
	rm /usr/local/bin/ntv_prun
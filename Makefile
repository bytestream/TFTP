#
# test suite for csc524.132 proj4
# author: bjr
# date: 30 mar 2013
#

MYTFTP= mytftp
HOST= localhost

TF= 511.dat 512.dat 513.dat empty.dat 1025.dat 1537.dat logfile.dat large.dat
SHELL= /bin/bash

setup:
	-killall ${MYTFTP}
	-rm -rf client
	mkdir client
	-rm -rf server
	mkdir server
	tar xf files.tar -C server
	cp ../${MYTFTP} server
	cp ../${MYTFTP} client
	cp Makefile server
	cp Makefile client

run:
	cd server ; make run-server
	cd client ; make run-client
	cd server ; make run-compare

run-server:
	./${MYTFTP} -l &

run-client:
	for testfile in ${TF} ; do \
	echo Reading $$testfile ; \
	./${MYTFTP} -r $$testfile ${HOST} ;\
	mv $$testfile $$testfile.return  ;\
	echo Writing $$testfile ;\
	./${MYTFTP} -w $$testfile.return ${HOST} ;\
	done

run-compare:
	for testfile in ${TF} ; do \
	cmp $$testfile $$testfile.return ;\
	done


clean:
	-killall ${MYTFTP}
	-rm -rf client
	-rm -rf server

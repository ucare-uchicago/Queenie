testGeneric: testGeneric.c
	gcc ioutil.c testGeneric.c -pthread -o testGeneric.out

testRead: testRead.c
	gcc ioutil.c testRead.c -pthread -o testRead.out

randomWrite: randomWrite.c
	gcc ioutil.c randomWrite.c -pthread -o randomWrite.out

sequentialWrite: sequentialWrite.c
	gcc ioutil.c sequentialWrite.c -pthread -o sequentialWrite.out

concurrentSeqWrite: concurrentSeqWrite.c
	gcc ioutil.c concurrentSeqWrite.c -pthread -o concurrentSeqWrite.out
	
intervalConSeqW: intervalConSeqW.c
	gcc ioutil.c intervalConSeqW.c -pthread -o intervalConSeqW.out

pushSectors: pushSectors.c
	gcc ioutil.c pushSectors.c -pthread -o pushSectors.out

incrementRead: incrementRead.c
	gcc ioutil.c incrementRead.c -pthread -o incrementRead.out

strideRead: strideRead.c
	gcc ioutil.c strideRead.c -pthread -o strideRead.out

rangeRead: rangeRead.c
	gcc ioutil.c rangeRead.c -pthread -o rangeRead.out

reReadFirst: reReadFirst.c
	gcc ioutil.c reReadFirst.c -pthread -o reReadFirst.out

repetitionRead: repetitionRead.c
	gcc ioutil.c repetitionRead.c -pthread -o repetitionRead.out

CephCOW: CephCOW.c
	gcc ioutil.c CephCOW.c -pthread -o CephCOW

conCephCOW: conCephCOW.c
	gcc ioutil.c conCephCOW.c -pthread -o conCephCOW

ioVerifier: ioVerifier.c
	gcc ioutil.c ioVerifier.c -pthread -o ioVerifier

cowVerifier: cowVerifier.c
	gcc ioutil.c cowVerifier.c -pthread -o cowVerifier

cowSeqVerifier: cowSeqVerifier.c
	gcc ioutil.c cowSeqVerifier.c -pthread -o cowSeqVerifier

rwVerifier: rwVerifier.c
	gcc ioutil.c rwVerifier.c -pthread -o rwVerifier

singleCOW: singleCOW.c
	gcc ioutil.c singleCOW.c -pthread -o singleCOW

conCOW: conCOW.c
	gcc ioutil.c conCOW.c -pthread -o conCOW

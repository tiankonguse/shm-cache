
当我们调整共享内存的大小之后, 发现运行失败了. 这时需要自己先手动删除

```
tiankonguse:readwrite $ ./write 
key=50462721
iShmID = 7766027
tiankonguse:readwrite $ ./read 
key=50462721
iShmID = 7766027
i=0 sec=1441278904 usec=644289
i=1 sec=1441278904 usec=644299
i=2 sec=1441278904 usec=644299

tiankonguse:readwrite $ make -B
g++ -o read read.cpp -I../../../include/  -L../../../lib/ -static -lshm -pthread
g++ -o write write.cpp -I../../../include/  -L../../../lib/ -static -lshm -pthread
tiankonguse:readwrite $ ./write 
key=50462721
getShm error. err=shmget error. ret=22

tiankonguse:readwrite $ ipcs -m | grep 7766027
0x03020001 7766027    tiankongus 666        24         0                       
tiankonguse:readwrite $ ipcrm -m 7766027
tiankonguse:readwrite $ ./write 
key=50462721
iShmID = 7798795
tiankonguse:readwrite $ ./read 
key=50462721
iShmID = 7798795
i=0 sec=1441279262 usec=344124
i=1 sec=1441279263 usec=344291
i=2 sec=1441279264 usec=34438
```

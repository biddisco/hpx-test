@echo off

rem Path=D:\build\hpx\Debug\lib\hpx;C:\Boost\lib;c:\Program Files\HDF5_1_8_cmake-vs11\bin
rem cd /d d:\build\hpx-my

rem Get N-1 from argument
set /a N=%1-1

rem set flag needed for my testing
set DEBUG_IDE=1

echo "Starting %1 instances as part of HPX job"
FOR /l %%x IN (0, 1, %N%) DO (
  rem use "start /B" to suppress windows per task 
  echo start /B D:\build\hpx-my\Debug\bin\benchmark_network_storage.exe -l%1 -%%x --hpx:run-hpx-main -Ihpx.parcel.tcp.enable=1 -Ihpx.parcel.async_serialization=1 --hpx:threads=4 -Ihpx.agas.max_pending_refcnt_requests=0
  
  start /B D:\build\hpx-my\Debug\bin\benchmark_network_storage.exe -l%1 -%%x --hpx:run-hpx-main -Ihpx.parcel.tcp.enable=1 -Ihpx.parcel.async_serialization=1 --hpx:threads=4 -Ihpx.agas.max_pending_refcnt_requests=0
)
 
rem 2097152 65536
rem
rem -Ihpx.parcel.async_serialization=0
rem  -Ihpx.parcel.tcp.enable=1  -Ihpx.parcel.bootstrap=mpi --hpx:debug-clp  --hpx:list-component-types
rem  hpx.agas.max_pending_refcnt_requests=
rem  -Ihpx.parcel.async_serialization=1 --hpx:threads=2 -Ihpx.threadpools.parcel_pool_size=2

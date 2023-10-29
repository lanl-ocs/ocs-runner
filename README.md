**Simple device-level OCS test client for performance evaluation and comparison**

OCS Runner
================

```
XX              XXXXX XXX         XX XX           XX       XX XX XXX         XXX
XX             XXX XX XXXX        XX XX           XX       XX XX    XX     XX   XX
XX            XX   XX XX XX       XX XX           XX       XX XX      XX XX       XX
XX           XX    XX XX  XX      XX XX           XX       XX XX      XX XX       XX
XX          XX     XX XX   XX     XX XX           XX XXXXX XX XX      XX XX       XX
XX         XX      XX XX    XX    XX XX           XX       XX XX     XX  XX
XX        XX       XX XX     XX   XX XX           XX       XX XX    XX   XX
XX       XX XX XX XXX XX      XX  XX XX           XX XXXXX XX XX XXX     XX       XX
XX      XX         XX XX       XX XX XX           XX       XX XX         XX       XX
XX     XX          XX XX        X XX XX           XX       XX XX         XX       XX
XX    XX           XX XX          XX XX           XX       XX XX          XX     XX
XXXX XX            XX XX          XX XXXXXXXXXX   XX       XX XX            XXXXXX
```

# Step 1: Compile SPDK with RDMA

```bash
git clone -b v23.09 --recursive https://github.com/spdk/spdk.git
cd spdk
export SPDK_DIR=`pwd` # Will use in step 2
sudo ./scripts/pkgdep.sh --rdma
./configure --with-rdma
make -j
```

# Step 2: Compile OCS Runner

```bash
git clone https://github.com/lanl-ocs/ocs-runner.git
cd ocs-runner
SPDK_DIR=${SPDK_DIR} make
```

# Step 3: Run the Runner

The idea is that the runner will read source object IDs from stdin (**one object ID per line**) and run queries against them one by one in order sequentially. Detailed usage information below.

```
$ ./ocs-runner -h
==============
Usage: sudo ./ocs-runner [options]

-t      trtype       :  Target transport type (e.g.: rdma)
-a      traddr       :  Target address (e.g.: 127.0.0.1)
-s      trsvcid      :  Service port (e.g.: 4420)
-n      subnqn       :  Name of subsystem
==============
```

Here is an example against an OCSA target subsystem running at `192.168.10.3`:`4420` via `RDMA` under the name `nqn.2023-10.gov.lanl:xxx:ssd1`. In addition, a parquet file object `xx_036785.parquet` has been prepopulated and has been assigned object ID `0`. See https://github.com/lanl-ocs/laghos-sample-dataset for data scheme and query information.

```bash
$ sudo ./ocs-runner -t rdma -a 192.168.10.3 -s 4420 -n nqn.2023-10.gov.lanl:xxx:ssd1 <<EOF
0
EOF
TELEMETRY: No legacy callbacks, legacy socket not created
Found RDMA+IPv4://192.168.10.3:4420/nqn.2023-10.gov.lanl:xxx:ssd1
Attached to nqn.2023-10.gov.lanl:xxx:ssd1
>> query plan submitted: id=0, result_size=488 B
>> query results obtained: 488 B / 488 B
VID,X,Y,Z,E
166462,1.5645773,1.5376104999999998,1.5803627999999998,1.4755714125000001
240662,1.5611519000000003,1.5749507000000003,1.5417632999999997,1.4774999125
219780,1.5264729999999997,1.5887712,1.5934969,1.4871300875
240660,1.503907,1.5975798999999997,1.5078072999999999,1.4890849375
159440,1.5057572,1.5352168,1.556996,1.49520745
233498,1.5047735,1.5526603,1.5391836,1.49543775
212414,1.5057572,1.5352168,1.556996,1.495636475
212409,1.5047735,1.5526603,1.5391836,1.4960388500000001
Total query time: 0.23 s
Total data read bytes: 488
Done!
```

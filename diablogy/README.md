Cassandra with VLOG



## Util directory
it includes simple python program that access cassandra db
please change the ip address to 155.230.91.227 


## Requirements

- Two Machine
  - For Cassandra DB
    - Java 8
    - Python 2.7.11+
  - For Collecting Log (VLOG)
    - Cassandra.h
    - https://gitlab.com/bctak/diablog
    - build-essential







### Setting up the Cassandra DB

https://www.vultr.com/docs/how-to-install-apache-cassandra-3-11-x-on-ubuntu-16-04-lts

*** Few Modification that you have to do

```
$nano /etc/cassandra/cassandra.yaml
```

In this yaml file please change

```
From : start_rpc: false —> To : start_rpc: true

From : rpc_address: localhost —> To : rpc_address: 0.0.0.0

From : broadcast_rpc_address: 0.0.0.0 —> To : broadcast_rpc_address: 155.230.91.227
```

Test your __cqlsh__ and verify that it works

**Q: Test what exactly? Just whether cqlsh runs? And, How?**





### Creating Keyspace and Table

Following script is for creating keyspace __beta__

```
$cqlsh
$cqlsh> CREATE KEYSPACE beta
   ... WITH REPLICATION = {
   ... 'class': 'SimpleStrategy', 'replication_factor': '2'
   ... };

```



Follwoing script is for creating table inside of beta with their value type

```
CREATE TABLE beta.fullstack (id int PRIMARY KEY, start_time double, end_time double, turnaround_time double, proc_name varchar, host_name varchar, proc_id varchar, thread_id varchar, sys_call varchar, return_byte int, fd int, sock_type varchar, pipe_val varchar, contents varchar, valid varchar);
```

__Finished__







### Setting up the Vlog (on different computer)

In order to use cassandra.h, it requires some packages and libraries. To install follow the instruction (more information visit https://github.com/datastax/cpp-driver)

Firstly,

```
$sudo apt update
$sudo apt cmake
```



Secondly, Download libuv and libuv-dev through following link http://downloads.datastax.com/cpp-driver/ubuntu/16.04/dependencies/libuv/v1.24.0/

```
$wget http://downloads.datastax.com/cpp-driver/ubuntu/16.04/dependencies/libuv/v1.24.0/libuv1_1.24.0-1_amd64.deb

$wget http://downloads.datastax.com/cpp-driver/ubuntu/16.04/dependencies/libuv/v1.24.0/libuv1-dev_1.24.0-1_amd64.deb

$sudo apt install ./libuv1_1.24.0-1_amd64.deb

$sudo apt install ./libuv1-dev_1.24.0-1_amd64.deb
```





Thirdly, Download cassandra driver for c/c++ through following linkhttp://downloads.datastax.com/cpp-driver/ubuntu/16.04/cassandra/v2.11.0/

```
$wget http://downloads.datastax.com/cpp-driver/ubuntu/16.04/cassandra/v2.11.0/cassandra-cpp-driver_2.11.0-1_amd64.deb

$wget http://downloads.datastax.com/cpp-driver/ubuntu/16.04/cassandra/v2.11.0/cassandra-cpp-driver-dev_2.11.0-1_amd64.deb

$sudo apt install ./cassandra-cpp-driver_2.11.0-1_amd64.deb

$sudo apt install ./cassandra-cpp-driver-dev_2.11.0-1_amd64.deb
```



Now we need to edit some codes that we cloned

```
$git clone https://gitlab.com/bctak/diablog
$cd diablog/bluecoat/src
```

1. Edit vlog.c file

   ```
   char* cassandra_serv_ip = "155.230.91.227"; //cassandra설치된 아이피 주소
   strcpy(name_of_db_table, "beta.openstack"); //맞는 keyspace와 테이블 지정
   ```



Compile the code

```
$sudo make all //in src directory
$sudo ./vlog
```

On different terminal

```
$sudo nano /etc/ld.so.preload
write
/lib64/libbluecoat.so
```

If you start running some command line the terminal that runs vlog will show something like below

![alt text](https://i.imgur.com/FU9HniF.png)



and you can check if it actually goes into cassandra by running

```
$cqlsh ip_address
cswl> SELECT * FROM beta.openstack;
```

![alt text](https://i.imgur.com/2oUWOz8.png)





### Retreving data using python instead of sqlsh

Installation is as follows

https://datastax.github.io/python-driver/installation.html



Edit the code as your taste and environment

```
self.keyspaces = "beta"
and
	def select_data(self):
		rows = self.session.execute('select * from openstack;')
		for row in rows:
			print("id = "+str(row.id)+ "  " + "proc_name = " + row.proc_name + "   "+"thread_id = "+ row.thread_id)
```

```
$python get_data.py
```

This will produce the output as follows.

![alt text](https://i.imgur.com/hymyZ7J.png)

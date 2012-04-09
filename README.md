Freeciv 2.3.2 -- with enhanced features
---------------------------------------

This is an enhanced version of Freeciv 2.3.2 with some special improvements:

*  Client shows the total production of the nation
*  Client writes unit activity into MySQL table aswell as to file freeciv-live-unit.log

Note! The current version might contain serious buffer overflow bugs. Use at 
your own risk! I'll audit the code once I am feeling alert again.

MySQL configurations
====================

Create file ~/.my.cnf and add these lines there:

	[freeciv_client]
	user = username
	password = yoursecretpassword
	host = localhost
	database = dbname

MySQL example
=============

The current version only writes (foreign) unit activity into freeciv_client_unitlog. I'll implement more soon. Luckily I already did most of the heavy 
lifting and it's really easy now.

	mysql> SELECT * FROM freeciv_client_unitlog;
	+------------+---------------------+-----------+--------+--------+------------------+---------+--------------+---------+
	| unitlog_id | created             | unit_name | unit_x | unit_y | player_name      | unit_hp | unit_veteran | unit_id |
	+------------+---------------------+-----------+--------+--------+------------------+---------+--------------+---------+
	|          1 | 0000-00-00 00:00:00 | Trireme   |     60 |      1 | Domenico Fattori |      10 |            0 |     164 |
	|          2 | 0000-00-00 00:00:00 | Trireme   |     67 |     14 | Domenico Fattori |      10 |            1 |     397 |
	|          3 | 0000-00-00 00:00:00 | Trireme   |     66 |     15 | Domenico Fattori |      10 |            1 |     397 |
	|          4 | 0000-00-00 00:00:00 | Trireme   |     65 |     16 | Domenico Fattori |      10 |            1 |     397 |
	+------------+---------------------+-----------+--------+--------+------------------+---------+--------------+---------+
	4 rows in set (0.00 sec)

Original README
===============

Please see the doc/ directory for a complete list of documentation files.

Freeciv 2.3.2 -- with enhanced features
---------------------------------------

This is an enhanced version of Freeciv 2.3.2 with some special improvements:

Freeciv Client
 * Shows the total production of the nation
 * Writes unit activity into MySQL and filesystem

MySQL configurations
====================

Create file ~/.my.cnf and add these lines there:

	[freeciv_client]
	user = username
	password = yoursecretpassword
	host = localhost
	database = dbname

Original README
===============

Please see the doc/ directory for a complete list of documentation files.

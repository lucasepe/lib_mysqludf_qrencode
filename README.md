lib_mysqludf_qrencode
=====================

Create QRCode images from your MySQL queries

Install
=======

Dependencies
------------

 * [libpng](http://www.libpng.org/pub/png/libpng.html)

 * [libqrencode](http://fukuchi.org/works/qrencode/index.en.html)

To compile
----------

    gcc -shared -o lib_mysqludf_qrencode.so lib_mysqludf_qrencode.c $(mysql_config --cflags)\
	   -lpng -lqrencode -DMYSQL_DYNAMIC_PLUGIN

To register this function
-------------------------

At mysql commandline prompt, type:

    CREATE FUNCTION qrencode RETURNS STRING SONAME 'lib_mysqludf_qrencode.so';
    CREATE FUNCTION qrencode_info RETURNS STRING SONAME 'lib_mysqludf_qrencode.so';

To get rid of the function
--------------------------

At mysql commandline prompt, type:

    DROP FUNCTION qrcode;
    DROP FUNCTION qrcode_info;


To list all installed functions
-------------------------------

At mysql commandline prompt, type__

    SELECT  FROM mysql.func;

How to use
==========

Once installed, to create a QRCode (PNG image) on the fly:

    SELECT qrencode( NAME ) FROM BEERS WHERE id=1;

The `SELECT` will return a blob containing your PNG image.

Python script showing how to use MySQL qrencode UDF
---------------------------------------------------

    import MySQLdb as mdb 
    import sys

    try:
        conn = mdb.connect( host='localhost',user='root', passwd='secret', db='MY_DATABASE')
        cursor = conn.cursor()

        cursor.execute("SELECT qrencode('http://www.geek03.com')")

        fout = open('image.png','wb')
        fout.write(cursor.fetchone()[0])
        fout.close()

        cursor.close()
        conn.close()

    except IOError, e:
        print "Error %d: %s" % (e.args[0],e.args[1])


_Maybe it will be useless or maybe not! who knows! I wrote this custom MySQL UDF just for fun...let me know if you like it!




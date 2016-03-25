PostgreSQL support for GDAL VFK driver
======================================

How to compile
--------------

        svn co https://svn.osgeo.org/gdal/trunk/gdal
        git clone https://github.com/ctu-yfsg/2016-a-gdal-vfk-pg.git
        cd gdal
        
        (cd ogr/ogrsf_frmts/ &&
         mv vfk vfk.orig &&
         ln -sf ../../../2016-a-gdal-vfk-pg/vfk .
        )
        
        ./configure --prefix=/usr/local --with-sqlite3
        make
        sudo make install